#ifndef TTS_FRONTEND_H
#define TTS_FRONTEND_H

#include <stdint.h>
#include <stddef.h>

#define TTS_MAX_WORDS      128u
#define TTS_MAX_PHONES     512u
#define TTS_MAX_FRAMES     4096u

typedef struct {
    uint8_t phones[TTS_MAX_PHONES];
    uint8_t stress[TTS_MAX_PHONES];
    size_t phone_count;

    uint8_t frame_phones[TTS_MAX_FRAMES];
    uint8_t frame_stress[TTS_MAX_FRAMES];
    size_t frame_count;
} TtsFrontendResult;

int tts_frontend_process(const char *text, TtsFrontendResult *out, int verbose);
void tts_debug_print(const char *text);

#endif
