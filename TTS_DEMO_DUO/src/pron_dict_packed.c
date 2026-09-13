/*
 * pron_dict_packed.c
 *
 * Lookup against the tools/pack_dict.py blob, used in place. Binary
 * search over bucket heads, then a linear decode inside one bucket.
 */

#include "pron_dict.h"

#include <stdint.h>

#define PDK_MAGIC 0x324B4450u
#define PDK_MAX_WORD 32
#define PDK_MAX_PHONES 32

typedef struct {
    uint32_t magic;
    uint32_t n_entries;
    uint32_t n_buckets;
    uint32_t bucket_size;
    uint32_t off_index;
    uint32_t off_entries;
    uint32_t total_bytes;
    uint32_t off_heads;
} PdkHeader;

static const uint8_t *pdk_blob;
static PdkHeader pdk_hdr_copy;
static const PdkHeader *pdk_hdr;
static const uint8_t *pdk_index;
static const uint8_t *pdk_entries;

/* pdk_read NULL means memory mapped. Card backed costs one read per word. */
static PronDictRead pdk_read;
static void *pdk_ctx;
static uint32_t pdk_entries_base;
static const uint8_t *pdk_heads;
static uint8_t *pdk_bucket;          /* caller supplied scratch */
static uint32_t pdk_bucket_cap;
static uint32_t pdk_bucket_off;      /* which bucket is loaded */
static int pdk_bucket_valid;

/* decode buffers, reused across calls; the API returns pointers to them */
static uint8_t pdk_phones[PDK_MAX_PHONES];
static uint8_t pdk_stress[PDK_MAX_PHONES];
static char pdk_word[PDK_MAX_WORD];
static PronEntry pdk_result;

static uint32_t pdk_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* index_ram needs 4 * n_buckets bytes, scratch needs the largest bucket. */
int pron_dict_init_stream(PronDictRead rd, void *ctx,
                          void *index_ram, uint32_t index_bytes,
                          void *scratch, uint32_t scratch_bytes)
{
    PdkHeader *h = &pdk_hdr_copy;

    if (!rd) return -1;
    if (rd(ctx, 0, h, sizeof(*h)) != 0) return -1;
    if (h->magic != PDK_MAGIC) return -2;
    if (h->bucket_size == 0 || h->n_buckets == 0) return -4;
    if (index_bytes < 4u * h->n_buckets) return -5;
    if (scratch_bytes < 512u) return -6;
    if (rd(ctx, h->off_index, index_ram, 4u * h->n_buckets) != 0) return -1;

    pdk_blob = 0;
    pdk_hdr = h;
    pdk_index = (const uint8_t *)index_ram;
    pdk_heads = (const uint8_t *)index_ram + 4u * h->n_buckets;
    if (index_bytes < 4u * h->n_buckets + (h->off_entries - h->off_heads))
        return -5;
    if (rd(ctx, h->off_heads, (uint8_t *)index_ram + 4u * h->n_buckets,
           h->off_entries - h->off_heads) != 0) return -1;
    pdk_entries = 0;
    pdk_entries_base = h->off_entries;
    pdk_read = rd;
    pdk_ctx = ctx;
    pdk_bucket = (uint8_t *)scratch;
    pdk_bucket_cap = scratch_bytes;
    pdk_bucket_valid = 0;
    return 0;
}

/* Point at the bucket at offset off, fetching it if the backend is a callback. */
static const uint8_t *pdk_bucket_at(uint32_t off)
{
    if (!pdk_read) return pdk_entries + off;
    if (pdk_bucket_valid && pdk_bucket_off == off) return pdk_bucket;
    if (pdk_read(pdk_ctx, pdk_entries_base + off, pdk_bucket,
                 pdk_bucket_cap) != 0) return 0;
    pdk_bucket_off = off;
    pdk_bucket_valid = 1;
    return pdk_bucket;
}

int pron_dict_init(const void *blob, uint32_t len)
{
    pdk_read = 0;
    const PdkHeader *h = (const PdkHeader *)blob;

    if (!blob || len < sizeof(PdkHeader)) return -1;
    if (h->magic != PDK_MAGIC) return -2;
    if (h->total_bytes > len) return -3;
    if (h->bucket_size == 0 || h->n_buckets == 0) return -4;

    pdk_blob = (const uint8_t *)blob;
    pdk_hdr = h;
    pdk_index = pdk_blob + h->off_index;
    pdk_heads = pdk_blob + h->off_heads;
    pdk_entries = pdk_blob + h->off_entries;
    return 0;
}

/* Decode one entry into pdk_word, extending the prefix. Returns the next. */
static const uint8_t *pdk_decode(const uint8_t *p, uint32_t *word_len,
                                 uint32_t *n_phones, int want_phones)
{
    uint32_t h = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    uint32_t shared = h & 31u;
    uint32_t wlen = (h >> 5) & 31u;
    uint32_t np = (h >> 10) & 31u;
    uint32_t i;

    p += 2;
    for (i = shared; i < wlen; i++) pdk_word[i] = (char)*p++;
    pdk_word[wlen] = 0;

    if (want_phones) {
        for (i = 0; i < np; i++) {
            uint8_t v = p[i];
            pdk_phones[i] = (uint8_t)(v & 63u);
            pdk_stress[i] = (uint8_t)(v >> 6);
        }
    }
    p += np;

    *word_len = wlen;
    *n_phones = np;
    return p;
}

/* Decode the head word of bucket b into pdk_word. */
static void pdk_head(uint32_t b)
{
    const uint8_t *p = pdk_heads;
    uint32_t i, k;
    for (i = 0; i <= b; i++) {
        uint32_t shared = p[0];
        uint32_t wlen = p[1];
        p += 2;
        for (k = shared; k < wlen; k++) pdk_word[k] = (char)*p++;
        pdk_word[wlen] = 0;
    }
}

static int pdk_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

const PronEntry *pron_lookup(const char *word)
{
    uint32_t lo, hi, wlen, np;
    const uint8_t *p;

    if (!pdk_hdr) return 0;

    /* The heads are front coded, so a probe walks forward rather than jumping. */
    lo = 0;
    hi = pdk_hdr->n_buckets;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        pdk_head(mid);
        if (pdk_strcmp(pdk_word, word) <= 0) lo = mid + 1u;
        else hi = mid;
    }
    if (lo == 0) return 0;
    lo--;

    /* linear decode through that bucket, carrying the shared prefix */
    p = pdk_bucket_at(pdk_u32(pdk_index + 4u * lo));
    if (!p) return 0;
    {
        uint32_t first = lo * pdk_hdr->bucket_size;
        uint32_t count = pdk_hdr->n_entries - first;
        uint32_t k;
        if (count > pdk_hdr->bucket_size) count = pdk_hdr->bucket_size;

        for (k = 0; k < count; k++) {
            p = pdk_decode(p, &wlen, &np, 1);
            if (pdk_strcmp(pdk_word, word) == 0) {
                pdk_result.word = pdk_word;
                pdk_result.phones = pdk_phones;
                pdk_result.stress = pdk_stress;
                pdk_result.phone_count = (uint8_t)np;
                return &pdk_result;
            }
        }
    }
    return 0;
}

uint32_t pron_dict_entries(void)
{
    return pdk_hdr ? pdk_hdr->n_entries : 0u;
}
