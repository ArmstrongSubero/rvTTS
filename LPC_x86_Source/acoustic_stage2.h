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

int acoustic_stage2_load(AcousticStage2 *m, const char *weights_path);
void acoustic_stage2_free(AcousticStage2 *m);

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
