/*
 * word_vocab.c
 *
 * Loads word_vocab.json and resolves a word to its prosody model
 * embedding ID. Linear search, run once in the frontend.
 */

#include "word_vocab.h"
#include "phonemes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORD_LEN 64

typedef struct {
    char word[MAX_WORD_LEN];
    uint16_t id;
} WordEntry;

static WordEntry *g_vocab = NULL;
static uint32_t g_vocab_count = 0;
static int g_loaded = 0;

static void skip_whitespace(const char **p)
{
    while (**p && (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r'))
        (*p)++;
}

static int parse_string(const char **p, char *out, size_t max_len)
{
    skip_whitespace(p);
    if (**p != '"') return -1;
    (*p)++;
    size_t i = 0;
    while (**p && **p != '"' && i < max_len - 1) {
        if (**p == '\\') {
            (*p)++;
            if (**p) out[i++] = **p;
        } else {
            out[i++] = **p;
        }
        (*p)++;
    }
    out[i] = '\0';
    if (**p == '"') (*p)++;
    return 0;
}

static int parse_int(const char **p, int *val)
{
    skip_whitespace(p);
    char *end;
    long v = strtol(*p, &end, 10);
    if (end == *p) return -1;
    *val = (int)v;
    *p = end;
    return 0;
}

int word_vocab_load(const char *json_path)
{
    if (g_loaded) return 0;

    FILE *f = fopen(json_path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open word vocab: %s\n", json_path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);

    /* One colon per entry */
    uint32_t count = 0;
    for (long i = 0; i < sz; i++) {
        if (buf[i] == ':') count++;
    }

    g_vocab = (WordEntry *)malloc(count * sizeof(WordEntry));
    if (!g_vocab) { free(buf); return -1; }

    const char *p = buf;
    skip_whitespace(&p);
    if (*p == '{') p++;

    g_vocab_count = 0;
    while (*p && *p != '}') {
        skip_whitespace(&p);
        if (*p == ',' || *p == '\n') { p++; continue; }
        if (*p == '}') break;

        char word[MAX_WORD_LEN];
        int id;

        if (parse_string(&p, word, MAX_WORD_LEN) != 0) break;
        skip_whitespace(&p);
        if (*p == ':') p++;
        if (parse_int(&p, &id) != 0) break;

        if (g_vocab_count < count) {
            strncpy(g_vocab[g_vocab_count].word, word, MAX_WORD_LEN - 1);
            g_vocab[g_vocab_count].word[MAX_WORD_LEN - 1] = '\0';
            g_vocab[g_vocab_count].id = (uint16_t)id;
            g_vocab_count++;
        }

        skip_whitespace(&p);
        if (*p == ',') p++;
    }

    free(buf);
    g_loaded = 1;

    printf("Word vocab loaded: %u words\n", (unsigned)g_vocab_count);
    return 0;
}

void word_vocab_free(void)
{
    if (g_vocab) free(g_vocab);
    g_vocab = NULL;
    g_vocab_count = 0;
    g_loaded = 0;
}

uint16_t word_vocab_lookup(const char *word)
{
    if (!g_loaded || !g_vocab || !word) return 0;

    char clean[MAX_WORD_LEN];
    size_t j = 0;
    for (size_t i = 0; word[i] && j < MAX_WORD_LEN - 1; i++) {
        char c = word[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '-') {
            clean[j++] = c;
        }
    }
    clean[j] = '\0';

    while (j > 0 && (clean[j-1] == '\'' || clean[j-1] == '-')) {
        clean[--j] = '\0';
    }

    if (j == 0) return 0;

    for (uint32_t i = 0; i < g_vocab_count; i++) {
        if (strcmp(g_vocab[i].word, clean) == 0) {
            return g_vocab[i].id;
        }
    }

    return 0; /* UNK */
}

void word_vocab_assign_frames(
    const char *text,
    const uint8_t *frame_phones,
    uint32_t frame_count,
    uint16_t *out_word_ids
) {
    (void)frame_phones;
    word_vocab_assign_frames_indexed(text, 0, frame_count, out_word_ids);
}

/* Frame to word id. Pass frame_word 0 to use the silence transitions. */
void word_vocab_assign_frames_indexed(
    const char *text,
    const uint16_t *frame_word,
    uint32_t frame_count,
    uint16_t *out_word_ids
) {
    char buf[4096];
    uint16_t ids[512];
    uint32_t n_words = 0;
    char *tok;
    uint32_t t;

    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    tok = strtok(buf, " \t\n\r");
    while (tok && n_words < 512) {
        ids[n_words++] = word_vocab_lookup(tok);
        tok = strtok(NULL, " \t\n\r");
    }

    for (t = 0; t < frame_count; t++) {
        uint16_t w = frame_word ? frame_word[t] : 0xFFFFu;
        out_word_ids[t] = (w < n_words) ? ids[w] : 0;
    }
}

