#ifndef PRON_DICT_H
#define PRON_DICT_H

#include <stdint.h>

typedef struct {
    const char *word;
    const uint8_t *phones;
    const uint8_t *stress;
    uint8_t phone_count;
} PronEntry;

/* Bind a tools/pack_dict.py blob, used in place. Returns 0 on success. */
int pron_dict_init(const void *blob, uint32_t len);

/* Number of entries in the bound dictionary, 0 if none is bound. */
uint32_t pron_dict_entries(void);

/* Look up an uppercase word. The pointers are valid until the next call. */
const PronEntry *pron_lookup(const char *word);

#endif
