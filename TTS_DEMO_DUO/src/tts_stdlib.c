/*
 * tts_stdlib.c
 *
 * bao_stdlib has no memmove and gcc emits a call to it for the
 * overlapping copy loop in lpcvoc.c. Everything else is inline in
 * tts_compat.h.
 */

#include <stddef.h>

#ifndef TTS_HOSTED

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *a = (unsigned char *)dst;
    const unsigned char *b = (const unsigned char *)src;

    if (a == b || n == 0) return dst;

    if (a < b) {
        while (n--) *a++ = *b++;
    } else {
        a += n;
        b += n;
        while (n--) *--a = *--b;
    }
    return dst;
}

#endif
