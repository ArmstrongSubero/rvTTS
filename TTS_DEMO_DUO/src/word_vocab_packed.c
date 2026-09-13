/*
 * word_vocab_packed.c
 *
 * Word to identifier map from a tools/pack_word_vocab.py blob, used in
 * place. Same front coded bucketed scheme as the pronunciation
 * dictionary, replacing 259 KB of JSON and a startup parser.
 */

#include "word_vocab.h"

#include <stdint.h>

#define WVK_MAGIC 0x314B5657u
#define WVK_MAX_WORD 32

typedef struct {
    uint32_t magic, n_entries, n_buckets, bucket_size;
    uint32_t off_index, off_entries, total_bytes, reserved;
} WvkHeader;

static const uint8_t *wvk_blob;
static const WvkHeader *wvk_hdr;
static const uint8_t *wvk_index;
static const uint8_t *wvk_entries;
static char wvk_word[WVK_MAX_WORD];

static uint32_t wvk_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int word_vocab_init(const void *blob, uint32_t len)
{
    const WvkHeader *h = (const WvkHeader *)blob;
    if (!blob || len < sizeof(WvkHeader)) return -1;
    if (h->magic != WVK_MAGIC) return -2;
    if (h->total_bytes > len) return -3;
    wvk_blob = (const uint8_t *)blob;
    wvk_hdr = h;
    wvk_index = wvk_blob + h->off_index;
    wvk_entries = wvk_blob + h->off_entries;
    return 0;
}

static const uint8_t *wvk_decode(const uint8_t *p, uint32_t *id)
{
    uint32_t h = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    uint32_t shared = h & 31u;
    uint32_t wlen = (h >> 5) & 31u;
    uint32_t i;
    p += 2;
    for (i = shared; i < wlen; i++) wvk_word[i] = (char)*p++;
    wvk_word[wlen] = 0;
    *id = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    return p + 2;
}

static int wvk_cmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/* Lowercase and strip punctuation. Apostrophe and hyphen survive inside a word. */
static void wvk_norm(const char *in, char *out)
{
    uint32_t n = 0;
    while (*in && n < WVK_MAX_WORD - 1) {
        char c = *in++;
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
            || c == '\'' || c == '-')
            out[n++] = c;
    }
    while (n > 0 && (out[n - 1] == '\'' || out[n - 1] == '-')) n--;
    out[n] = 0;
}

uint16_t word_vocab_lookup(const char *word)
{
    char key[WVK_MAX_WORD];
    uint32_t lo, hi, id = 0;
    const uint8_t *p;

    if (!wvk_hdr) return 0;
    wvk_norm(word, key);
    if (!key[0]) return 0;

    lo = 0; hi = wvk_hdr->n_buckets;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        wvk_decode(wvk_entries + wvk_u32(wvk_index + 4u * mid), &id);
        if (wvk_cmp(wvk_word, key) <= 0) lo = mid + 1u; else hi = mid;
    }
    if (lo == 0) return 0;
    lo--;

    p = wvk_entries + wvk_u32(wvk_index + 4u * lo);
    {
        uint32_t first = lo * wvk_hdr->bucket_size;
        uint32_t count = wvk_hdr->n_entries - first;
        uint32_t k;
        if (count > wvk_hdr->bucket_size) count = wvk_hdr->bucket_size;
        for (k = 0; k < count; k++) {
            p = wvk_decode(p, &id);
            if (wvk_cmp(wvk_word, key) == 0) return (uint16_t)id;
        }
    }
    return 0;
}

uint32_t word_vocab_size(void)
{
    return wvk_hdr ? wvk_hdr->n_entries : 0u;
}

/* Frame to word identifier, from the frontend's own per frame word index. */
void word_vocab_assign_frames_indexed(const char *text,
                                      const uint16_t *frame_word,
                                      uint32_t frame_count,
                                      uint16_t *out_word_ids)
{
    uint16_t ids[512];
    uint32_t n_words = 0;
    const char *p = text;
    uint32_t t;

    while (*p && n_words < 512) {
        char tok[WVK_MAX_WORD];
        uint32_t k = 0;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            if (k < WVK_MAX_WORD - 1) tok[k++] = *p;
            p++;
        }
        tok[k] = 0;
        ids[n_words++] = word_vocab_lookup(tok);
    }

    for (t = 0; t < frame_count; t++) {
        uint16_t w = frame_word ? frame_word[t] : 0xFFFFu;
        out_word_ids[t] = (w < n_words) ? ids[w] : 0;
    }
}
