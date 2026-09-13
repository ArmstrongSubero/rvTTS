#include "wav_writer.h"

#include <stdio.h>
#include <stdint.h>

static void write_u16_le(FILE *f, uint16_t v)
{
    unsigned char b[2];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    fwrite(b, 1, 2, f);
}

static void write_u32_le(FILE *f, uint32_t v)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    b[2] = (unsigned char)((v >> 16) & 0xff);
    b[3] = (unsigned char)((v >> 24) & 0xff);
    fwrite(b, 1, 4, f);
}

int wav_write_mono16(
    const char *path,
    const int16_t *samples,
    uint32_t sample_count,
    uint32_t sample_rate
) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
    uint16_t block_align = channels * bits_per_sample / 8;
    uint32_t byte_rate = sample_rate * block_align;
    uint32_t data_bytes = sample_count * block_align;
    uint32_t riff_size = 36 + data_bytes;

    fwrite("RIFF", 1, 4, f);
    write_u32_le(f, riff_size);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    write_u32_le(f, 16);              /* PCM fmt chunk size */
    write_u16_le(f, 1);               /* PCM format */
    write_u16_le(f, channels);
    write_u32_le(f, sample_rate);
    write_u32_le(f, byte_rate);
    write_u16_le(f, block_align);
    write_u16_le(f, bits_per_sample);

    fwrite("data", 1, 4, f);
    write_u32_le(f, data_bytes);

    fwrite(samples, sizeof(int16_t), sample_count, f);

    fclose(f);
    return 0;
}
