#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <stdint.h>

int wav_write_mono16(
    const char *path,
    const int16_t *samples,
    uint32_t sample_count,
    uint32_t sample_rate
);

#endif
