#ifndef TEXTNORM_H
#define TEXTNORM_H

#include <stddef.h>

#define MAX_WORD_LEN 48
#define MAX_WORDS    128

typedef struct {
    char words[MAX_WORDS][MAX_WORD_LEN];
    size_t count;
} WordList;

void normalize_and_tokenize(const char *text, WordList *out);

#endif /* TEXTNORM_H */
