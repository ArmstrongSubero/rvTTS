#ifndef RECITER_H
#define RECITER_H

#include <stdint.h>
#include <stddef.h>

size_t reciter_guess(const char *word, uint8_t *out, size_t max_out);

#endif /* RECITER_H */
