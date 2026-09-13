/*
 * word_vocab.h
 *
 * Word vocabulary behind the prosody model word embeddings.
 */

#ifndef WORD_VOCAB_H
#define WORD_VOCAB_H

#include <stdint.h>

/* Call once at startup. */
int word_vocab_load(const char *json_path);

void word_vocab_free(void);

/* Returns 0 (UNK) for a word that is not in the vocabulary. */
int word_vocab_init(const void *blob, uint32_t len);
uint32_t word_vocab_size(void);
uint16_t word_vocab_lookup(const char *word);

/* Silence frames take UNK. Everything between two silences is one word. */
void word_vocab_assign_frames_indexed(
    const char *text,
    const uint16_t *frame_word,
    uint32_t frame_count,
    uint16_t *out_word_ids
);

void word_vocab_assign_frames(
    const char *text,
    const uint8_t *frame_phones,
    uint32_t frame_count,
    uint16_t *out_word_ids
);

#endif
