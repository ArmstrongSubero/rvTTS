#ifndef PRON_DICT_H
#define PRON_DICT_H

#include <stdint.h>

typedef struct {
    const char *word;
    const uint8_t *phones;
    const uint8_t *stress;
    uint8_t phone_count;
} PronEntry;

const PronEntry *pron_lookup(const char *word);

#endif
