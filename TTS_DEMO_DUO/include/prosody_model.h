/*
 * prosody_model.h
 *
 * Stage 1 prosody model, v5. Bidirectional GRU of 64 with word
 * embeddings, int8 only.
 */
#ifndef PROSODY_MODEL_H
#define PROSODY_MODEL_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
    uint8_t *blob;
    size_t blob_size;
    int owns_blob;   /* 1 if prosody_free should release blob */
    /* Int8 weight pointers */
    const int8_t *phone_emb_i8;
    const int8_t *stress_emb_i8;
    const int8_t *word_emb_i8;
    const int8_t *pre_w_i8;
    const int8_t *gru_fw_ih_w_i8;
    const int8_t *gru_fw_hh_w_i8;
    const int8_t *gru_bw_ih_w_i8;
    const int8_t *gru_bw_hh_w_i8;
    /* mu head */
    const int8_t *out_mu_w1_i8;
    const int8_t *out_mu_w2_i8;
    /* logvar head */
    const int8_t *out_lv_w1_i8;
    const int8_t *out_lv_w2_i8;
    /* Scale factors for int8 */
    float scale_phone_emb;
    float scale_stress_emb;
    float scale_word_emb;
    float scale_pre_w;
    float scale_gru_fw_ih_w;
    float scale_gru_fw_hh_w;
    float scale_gru_bw_ih_w;
    float scale_gru_bw_hh_w;
    float scale_out_mu_w1;
    float scale_out_mu_w2;
    float scale_out_lv_w1;
    float scale_out_lv_w2;
    /* Biases always float32 */
    const float *pre_b;
    const float *gru_fw_ih_b;
    const float *gru_fw_hh_b;
    const float *gru_bw_ih_b;
    const float *gru_bw_hh_b;
    const float *out_mu_b1;
    const float *out_mu_b2;
    const float *out_lv_b1;
    const float *out_lv_b2;
    /* Norm stats always float32 */
    const float *norm_mean;
    const float *norm_std;
    /* 0.0 is deterministic, 0.5 to 0.8 sounds natural */
    float temperature;
} ProsodyModel;
size_t prosody_scratch_bytes(uint32_t frame_count);
void prosody_set_scratch(void *mem, size_t bytes);
int prosody_load_mem(ProsodyModel *m, const void *blob, uint32_t len);
int prosody_load(ProsodyModel *m, const char *weights_path);
void prosody_free(ProsodyModel *m);
int prosody_generate(
    ProsodyModel *m,
    const uint8_t *frame_phones,
    const uint8_t *frame_stress,
    const uint16_t *frame_word_ids,
    uint32_t frame_count,
    float *out_energy,
    float *out_pitch,
    float *out_voicing
);
#endif
