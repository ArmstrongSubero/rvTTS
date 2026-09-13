/*
 * acoustic_stage2.h
 *
 * Stage 2 acoustic model: dilated conv conditioned on prosody, int8.
 */

#ifndef ACOUSTIC_STAGE2_H
#define ACOUSTIC_STAGE2_H

#include <stdint.h>
#include <stddef.h>

#define AS2_MAX_DILATED 5

typedef struct {
    uint8_t *blob;
    size_t blob_size;
    int owns_blob;

    /* Int8 weight pointers */
    const int8_t *phone_emb_i8;
    const int8_t *stress_emb_i8;
    const int8_t *conv_pre_w_i8;
    const int8_t *context_conv_w_i8;
    const int8_t *dilated_w_i8[AS2_MAX_DILATED];
    const int8_t *out_w1_i8;
    const int8_t *out_w2_i8;

    /* Scale factors for int8 (one per weight tensor) */
    float scale_phone_emb;
    float scale_stress_emb;
    float scale_conv_pre_w;
    float scale_context_conv_w;
    float scale_dilated_w[AS2_MAX_DILATED];
    float scale_out_w1;
    float scale_out_w2;

    /* Biases always float32 */
    const float *conv_pre_b;
    const float *dilated_b[AS2_MAX_DILATED];
    const float *out_b1;
    const float *out_b2;

    /* Norm stats always float32 */
    const float *norm_mean;
    const float *norm_std;
} AcousticStage2;

size_t acoustic_stage2_scratch_bytes(uint32_t frame_count);
void acoustic_stage2_set_scratch(void *mem, size_t bytes);
int acoustic_stage2_load_mem(AcousticStage2 *m, const void *blob, uint32_t len);
int acoustic_stage2_load(AcousticStage2 *m, const char *weights_path);
void acoustic_stage2_free(AcousticStage2 *m);

/* Chunked: arena sized for one 142 frame window, 214 KB not 376 KB. */
size_t   acoustic_stage2_chunk_scratch_bytes(void);
uint32_t acoustic_stage2_window_frames(void);

/*
 * Optional staging buffer, wants AS2_STAGE_BYTES. Without it the
 * kernels read int8 weights out of CodeFlash, 58 MB per window and
 * none of it contiguous. Output is bit identical either way. Put it in
 * ITCM at 0x200A0000 on the V5F.
 */
#define AS2_STAGE_BYTES 24576u

void acoustic_stage2_set_stage_buf(void *buf, size_t bytes);

int acoustic_stage2_generate_chunked(
    AcousticStage2 *m,
    const uint8_t *frame_phones,
    const uint8_t *frame_stress,
    const float *cond_energy,
    const float *cond_pitch,
    const float *cond_voicing,
    uint32_t frame_count,
    float *out_features,
    uint8_t *win_phones,
    uint8_t *win_stress,
    float *win_energy,
    float *win_pitch,
    float *win_voicing,
    float *win_out
);

int acoustic_stage2_generate(
    AcousticStage2 *m,
    const uint8_t *frame_phones,
    const uint8_t *frame_stress,
    const float *cond_energy,
    const float *cond_pitch,
    const float *cond_voicing,
    uint32_t frame_count,
    float *out_features
);

#endif