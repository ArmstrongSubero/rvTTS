/*
 * tts_log.c
 *
 * tts_compat.h routes diagnostics through here. printf is already on
 * USART1. Only the V3F prints. TTS_QUIET compiles every call out.
 */

#include <stdarg.h>
#include <stdio.h>

#ifndef TTS_QUIET
void tts_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
#endif
