#ifndef FARGAN_WRAPPER_H
#define FARGAN_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#define TTS_SAMPLE_RATE 16000u
#define TTS_FARGAN_FEATURE_DIM 20u

/* Comment out to go back to the float32 FARGAN GEMV. */
#define FARGAN_USE_INT8

/* Synthesise PCM16 from frame_count x 20 float32 features. Returns 0 on success. */
int tts_fargan_synthesize_pcm16(
    const float *features,
    uint32_t frame_count,
    int16_t *out_pcm,
    uint32_t max_samples,
    uint32_t *out_sample_count
);

#endif
