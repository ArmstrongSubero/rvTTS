/*
 * prosody_model_int8.c
 *
 * Stage 1 prosody model, v5, int8. Predicts a mean and a log variance
 * and samples from them. Temperature 0.0 gives the mean back.
 */

#include "prosody_model.h"
#include "prosody_weights_int8_meta.h"
#include "phonemes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static int pw_rng_seeded = 0;

static float pw_randn(void) {
    if (!pw_rng_seeded) { srand((unsigned)time(NULL)); pw_rng_seeded = 1; }
    float u1, u2;
    do { u1 = (float)rand() / (float)RAND_MAX; } while (u1 < 1e-10f);
    u2 = (float)rand() / (float)RAND_MAX;
    return sqrtf(-2.0f * logf(u1)) * cosf(6.2831853f * u2);
}

static int pw_is_vowel(uint8_t ph) {
    switch (ph) { case PH_AA: case PH_AE: case PH_AH: case PH_AO: case PH_AW: case PH_AY: case PH_EH: case PH_ER: case PH_EY: case PH_IH: case PH_IY: case PH_OW: case PH_OY: case PH_UH: case PH_UW: return 1; default: return 0; }
}
static int pw_is_fricative(uint8_t ph) {
    switch (ph) { case PH_F: case PH_V: case PH_S: case PH_Z: case PH_SH: case PH_ZH: case PH_TH: case PH_DH: case PH_HH: return 1; default: return 0; }
}
static int pw_is_stop(uint8_t ph) {
    switch (ph) { case PH_B: case PH_D: case PH_G: case PH_P: case PH_T: case PH_K: return 1; default: return 0; }
}
static int pw_is_affricate(uint8_t ph) { return ph == PH_CH || ph == PH_JH; }
static int pw_is_nasal(uint8_t ph) { return ph == PH_M || ph == PH_N || ph == PH_NG; }
static int pw_is_liquid(uint8_t ph) { return ph == PH_L || ph == PH_R; }
static int pw_is_glide(uint8_t ph) { return ph == PH_W || ph == PH_Y; }
static int pw_is_voiced(uint8_t ph) {
    return pw_is_vowel(ph) || pw_is_nasal(ph) || pw_is_liquid(ph) || pw_is_glide(ph) || ph == PH_B || ph == PH_D || ph == PH_G || ph == PH_V || ph == PH_Z || ph == PH_ZH || ph == PH_DH || ph == PH_JH;
}
static int pw_is_sonorant(uint8_t ph) { return pw_is_vowel(ph) || pw_is_nasal(ph) || pw_is_liquid(ph) || pw_is_glide(ph); }
static int pw_is_silence(uint8_t ph) { return ph == PH_SIL || ph == PH_BRK; }

static float pw_sigmoidf(float x) {
    if (x > 40.0f) return 1.0f;
    if (x < -40.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

static uint8_t pw_prev_phone(const uint8_t *fp, uint32_t t) {
    if (t == 0) return PH_SIL; uint8_t c = fp[t]; uint32_t i = t;
    while (i > 0 && fp[i-1] == c) i--; return (i == 0) ? PH_SIL : fp[i-1];
}
static uint8_t pw_next_phone(const uint8_t *fp, uint32_t t, uint32_t n) {
    uint8_t c = fp[t]; uint32_t i = t;
    while (i+1 < n && fp[i+1] == c) i++; return (i+1 >= n) ? PH_SIL : fp[i+1];
}
static uint32_t pw_seg_start(const uint8_t *fp, uint32_t t) {
    uint8_t ph = fp[t]; uint32_t s = t; while (s > 0 && fp[s-1] == ph) s--; return s;
}
static uint32_t pw_seg_end(const uint8_t *fp, uint32_t t, uint32_t n) {
    uint8_t ph = fp[t]; uint32_t e = t+1; while (e < n && fp[e] == ph) e++; return e;
}

static void pw_linear_i8(const int8_t *w, const float *b, float scale,
                          const float *x, uint32_t od, uint32_t id, float *y)
{
    for (uint32_t o = 0; o < od; o++) {
        const int8_t *row = w + (size_t)o * id;
        float acc = 0.0f;
        for (uint32_t i = 0; i < id; i++) acc += (float)row[i] * x[i];
        y[o] = acc * scale + (b ? b[o] : 0.0f);
    }
}

static void pw_emb_i8(const int8_t *e, float s, uint32_t id, uint32_t d, float *o)
{ const int8_t *r = e + (size_t)id * d; for (uint32_t i = 0; i < d; i++) o[i] = (float)r[i] * s; }

static void pw_gru_step_i8(const int8_t *w_ih, float s_ih,
                             const int8_t *w_hh, float s_hh,
                             const float *b_ih, const float *b_hh,
                             const float *x, uint32_t in_dim, uint32_t H,
                             float *h, float *tmp_hh, float *new_h)
{
    for (uint32_t g = 0; g < 3u * H; g++) {
        const int8_t *row = w_hh + (size_t)g * H;
        float acc = 0.0f;
        for (uint32_t j = 0; j < H; j++) acc += (float)row[j] * h[j];
        tmp_hh[g] = acc * s_hh + b_hh[g];
    }
    for (uint32_t j = 0; j < H; j++) {
        const int8_t *rr = w_ih + (size_t)j * in_dim;
        const int8_t *rz = w_ih + (size_t)(H+j) * in_dim;
        const int8_t *rn = w_ih + (size_t)(2u*H+j) * in_dim;
        float x_r = 0, x_z = 0, x_n = 0;
        for (uint32_t k = 0; k < in_dim; k++) {
            float xk = x[k];
            x_r += (float)rr[k] * xk;
            x_z += (float)rz[k] * xk;
            x_n += (float)rn[k] * xk;
        }
        x_r = x_r * s_ih + b_ih[j];
        x_z = x_z * s_ih + b_ih[H+j];
        x_n = x_n * s_ih + b_ih[2u*H+j];

        float r = pw_sigmoidf(x_r + tmp_hh[j]);
        float z = pw_sigmoidf(x_z + tmp_hh[H+j]);
        float n = tanhf(x_n + r * tmp_hh[2u*H+j]);
        new_h[j] = (1.0f - z) * n + z * h[j];
    }
    memcpy(h, new_h, H * sizeof(float));
}

int prosody_load(ProsodyModel *m, const char *weights_path)
{
    memset(m, 0, sizeof(*m));
    FILE *f = fopen(weights_path, "rb");
    if (!f) { fprintf(stderr, "Cannot open prosody weights: %s\n", weights_path); return -1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    m->blob = (uint8_t *)malloc((size_t)sz);
    if (!m->blob) { fclose(f); return -1; }
    fread(m->blob, 1, (size_t)sz, f); fclose(f);
    m->blob_size = (size_t)sz;

    uint32_t magic; memcpy(&magic, m->blob, 4);

    if (magic != PW8_WEIGHT_MAGIC) {
        fprintf(stderr, "Expected v5 prosody weights (magic 0x%08X), got 0x%08X\n",
                PW8_WEIGHT_MAGIC, magic);
        free(m->blob); memset(m, 0, sizeof(*m)); return -1;
    }

    m->phone_emb_i8   = (const int8_t *)(m->blob + PW8_OFF_PHONE_EMB);
    m->stress_emb_i8  = (const int8_t *)(m->blob + PW8_OFF_STRESS_EMB);
    m->word_emb_i8    = (const int8_t *)(m->blob + PW8_OFF_WORD_EMB);
    m->pre_w_i8       = (const int8_t *)(m->blob + PW8_OFF_PRE_W);
    m->pre_b          = (const float *)(m->blob + PW8_OFF_PRE_B);
    m->gru_fw_ih_w_i8 = (const int8_t *)(m->blob + PW8_OFF_GRU_FW_IH_W);
    m->gru_fw_hh_w_i8 = (const int8_t *)(m->blob + PW8_OFF_GRU_FW_HH_W);
    m->gru_fw_ih_b    = (const float *)(m->blob + PW8_OFF_GRU_FW_IH_B);
    m->gru_fw_hh_b    = (const float *)(m->blob + PW8_OFF_GRU_FW_HH_B);
    m->gru_bw_ih_w_i8 = (const int8_t *)(m->blob + PW8_OFF_GRU_BW_IH_W);
    m->gru_bw_hh_w_i8 = (const int8_t *)(m->blob + PW8_OFF_GRU_BW_HH_W);
    m->gru_bw_ih_b    = (const float *)(m->blob + PW8_OFF_GRU_BW_IH_B);
    m->gru_bw_hh_b    = (const float *)(m->blob + PW8_OFF_GRU_BW_HH_B);

    /* mu head */
    m->out_mu_w1_i8   = (const int8_t *)(m->blob + PW8_OFF_OUT_MU_W1);
    m->out_mu_b1      = (const float *)(m->blob + PW8_OFF_OUT_MU_B1);
    m->out_mu_w2_i8   = (const int8_t *)(m->blob + PW8_OFF_OUT_MU_W2);
    m->out_mu_b2      = (const float *)(m->blob + PW8_OFF_OUT_MU_B2);

    /* logvar head */
    m->out_lv_w1_i8   = (const int8_t *)(m->blob + PW8_OFF_OUT_LV_W1);
    m->out_lv_b1      = (const float *)(m->blob + PW8_OFF_OUT_LV_B1);
    m->out_lv_w2_i8   = (const int8_t *)(m->blob + PW8_OFF_OUT_LV_W2);
    m->out_lv_b2      = (const float *)(m->blob + PW8_OFF_OUT_LV_B2);

    m->norm_mean      = (const float *)(m->blob + PW8_OFF_NORM_MEAN);
    m->norm_std       = (const float *)(m->blob + PW8_OFF_NORM_STD);

    m->scale_phone_emb   = PW8_SCALE_PHONE_EMB;
    m->scale_stress_emb  = PW8_SCALE_STRESS_EMB;
    m->scale_word_emb    = PW8_SCALE_WORD_EMB;
    m->scale_pre_w       = PW8_SCALE_PRE_W;
    m->scale_gru_fw_ih_w = PW8_SCALE_GRU_FW_IH_W;
    m->scale_gru_fw_hh_w = PW8_SCALE_GRU_FW_HH_W;
    m->scale_gru_bw_ih_w = PW8_SCALE_GRU_BW_IH_W;
    m->scale_gru_bw_hh_w = PW8_SCALE_GRU_BW_HH_W;
    m->scale_out_mu_w1   = PW8_SCALE_OUT_MU_W1;
    m->scale_out_mu_w2   = PW8_SCALE_OUT_MU_W2;
    m->scale_out_lv_w1   = PW8_SCALE_OUT_LV_W1;
    m->scale_out_lv_w2   = PW8_SCALE_OUT_LV_W2;

    m->temperature = 0.0f;

    printf("Loaded prosody model v5 stochastic (int8, %zu bytes, temp=%.2f)\n",
           m->blob_size, m->temperature);
    return 0;
}

void prosody_free(ProsodyModel *m)
{
    if (m && m->blob) free(m->blob);
    if (m) memset(m, 0, sizeof(*m));
}

int prosody_generate(
    ProsodyModel *m, const uint8_t *frame_phones, const uint8_t *frame_stress,
    const uint16_t *frame_word_ids, uint32_t frame_count,
    float *out_energy, float *out_pitch, float *out_voicing)
{
    if (!m || !m->blob || !frame_phones || frame_count == 0) return -1;

    const uint32_t H = PW8_HIDDEN_DIM;
    const uint32_t PRE_IN = PW8_INPUT_DIM;
    const uint32_t OUT_DIM = PW8_OUTPUT_DIM;

    float *iv    = (float *)malloc(PRE_IN * sizeof(float));
    float *pre   = (float *)malloc((size_t)frame_count * H * sizeof(float));
    float *fwo   = (float *)malloc((size_t)frame_count * H * sizeof(float));
    float *bwo   = (float *)malloc((size_t)frame_count * H * sizeof(float));
    float *gh    = (float *)calloc(H, sizeof(float));
    float *thh   = (float *)malloc(3u * H * sizeof(float));
    float *nh    = (float *)malloc(H * sizeof(float));
    float *cat   = (float *)malloc(2u * H * sizeof(float));
    float *h64   = (float *)malloc(H * sizeof(float));
    float *h64_lv = (float *)malloc(H * sizeof(float));

    if (!iv||!pre||!fwo||!bwo||!gh||!thh||!nh||!cat||!h64||!h64_lv) {
        free(iv);free(pre);free(fwo);free(bwo);free(gh);free(thh);free(nh);free(cat);free(h64);free(h64_lv);
        return -1;
    }

    /* Input build and pre projection */
    for (uint32_t t = 0; t < frame_count; t++) {
        uint32_t p = 0;
        uint8_t cp = frame_phones[t], pp = pw_prev_phone(frame_phones, t), np = pw_next_phone(frame_phones, t, frame_count);
        if (cp >= PW8_PHONE_COUNT) cp = 0;
        if (pp >= PW8_PHONE_COUNT) pp = 0;
        if (np >= PW8_PHONE_COUNT) np = 0;

        pw_emb_i8(m->phone_emb_i8, m->scale_phone_emb, pp, PW8_PHONE_EMB_DIM, iv+p); p += PW8_PHONE_EMB_DIM;
        pw_emb_i8(m->phone_emb_i8, m->scale_phone_emb, cp, PW8_PHONE_EMB_DIM, iv+p); p += PW8_PHONE_EMB_DIM;
        pw_emb_i8(m->phone_emb_i8, m->scale_phone_emb, np, PW8_PHONE_EMB_DIM, iv+p); p += PW8_PHONE_EMB_DIM;
        uint8_t sid = frame_stress ? frame_stress[t] : 0; if (sid >= PW8_STRESS_COUNT) sid = 0;
        pw_emb_i8(m->stress_emb_i8, m->scale_stress_emb, sid, PW8_STRESS_EMB_DIM, iv+p); p += PW8_STRESS_EMB_DIM;
        uint16_t wid = frame_word_ids ? frame_word_ids[t] : 0; if (wid >= PW8_WORD_COUNT) wid = 0;
        pw_emb_i8(m->word_emb_i8, m->scale_word_emb, wid, PW8_WORD_EMB_DIM, iv+p); p += PW8_WORD_EMB_DIM;

        iv[p++]=(float)pw_is_vowel(cp); iv[p++]=(float)pw_is_fricative(cp);
        iv[p++]=(float)pw_is_stop(cp); iv[p++]=(float)pw_is_affricate(cp);
        iv[p++]=(float)pw_is_nasal(cp); iv[p++]=(float)pw_is_liquid(cp);
        iv[p++]=(float)pw_is_glide(cp); iv[p++]=(float)pw_is_sonorant(cp);
        iv[p++]=(float)pw_is_voiced(cp); iv[p++]=(float)pw_is_silence(cp);

        uint32_t ss=pw_seg_start(frame_phones,t), se=pw_seg_end(frame_phones,t,frame_count), sl=se-ss;
        float pos=(sl>1)?(float)(t-ss)/(float)(sl-1):0.0f;
        iv[p++]=pos; iv[p++]=1.0f-pos;
        float ds=(float)sl/30.0f; if(ds>2.0f)ds=2.0f;
        iv[p++]=ds; iv[p++]=logf(1.0f+(float)sl)/5.0f;

        float utt_pos=(float)t/(float)(frame_count>1?frame_count-1:1);
        float sf=0.0f; if(utt_pos>0.85f) sf=(utt_pos-0.85f)/0.15f;
        iv[p++]=utt_pos; iv[p++]=1.0f-utt_pos; iv[p++]=sf;
        iv[p++]=0.0f; iv[p++]=(float)frame_count*0.01f/30.0f;

        iv[p++]=pw_is_silence(cp)?0.0f:1.0f; iv[p++]=0.2f;
        iv[p++]=utt_pos; iv[p++]=pos; iv[p++]=0.0f;

        pw_linear_i8(m->pre_w_i8, m->pre_b, m->scale_pre_w, iv, H, PRE_IN, pre+(size_t)t*H);
        float *po = pre+(size_t)t*H;
        for (uint32_t j=0;j<H;j++) po[j]=tanhf(po[j]);
    }

    /* Forward GRU */
    memset(gh, 0, H*sizeof(float));
    for (uint32_t t=0; t<frame_count; t++) {
        pw_gru_step_i8(m->gru_fw_ih_w_i8, m->scale_gru_fw_ih_w,
                        m->gru_fw_hh_w_i8, m->scale_gru_fw_hh_w,
                        m->gru_fw_ih_b, m->gru_fw_hh_b,
                        pre+(size_t)t*H, H, H, gh, thh, nh);
        memcpy(fwo+(size_t)t*H, gh, H*sizeof(float));
    }

    /* Backward GRU */
    memset(gh, 0, H*sizeof(float));
    for (int32_t t=(int32_t)frame_count-1; t>=0; t--) {
        pw_gru_step_i8(m->gru_bw_ih_w_i8, m->scale_gru_bw_ih_w,
                        m->gru_bw_hh_w_i8, m->scale_gru_bw_hh_w,
                        m->gru_bw_ih_b, m->gru_bw_hh_b,
                        pre+(size_t)t*H, H, H, gh, thh, nh);
        memcpy(bwo+(size_t)t*H, gh, H*sizeof(float));
    }

    for (uint32_t t=0; t<frame_count; t++) {
        memcpy(cat, fwo+(size_t)t*H, H*sizeof(float));
        memcpy(cat+H, bwo+(size_t)t*H, H*sizeof(float));

        /* mu head */
        pw_linear_i8(m->out_mu_w1_i8, m->out_mu_b1, m->scale_out_mu_w1, cat, H, 2u*H, h64);
        for (uint32_t j=0;j<H;j++) h64[j]=tanhf(h64[j]);
        float mu[3];
        pw_linear_i8(m->out_mu_w2_i8, m->out_mu_b2, m->scale_out_mu_w2, h64, OUT_DIM, H, mu);

        /* logvar head */
        pw_linear_i8(m->out_lv_w1_i8, m->out_lv_b1, m->scale_out_lv_w1, cat, H, 2u*H, h64_lv);
        for (uint32_t j=0;j<H;j++) h64_lv[j]=tanhf(h64_lv[j]);
        float lv[3];
        pw_linear_i8(m->out_lv_w2_i8, m->out_lv_b2, m->scale_out_lv_w2, h64_lv, OUT_DIM, H, lv);

        /* out = mu + exp(0.5 * logvar) * noise * temperature */
        float raw[3];
        for (uint32_t d = 0; d < OUT_DIM; d++) {
            float log_var = lv[d];
            if (log_var < -6.0f) log_var = -6.0f;
            if (log_var > 2.0f) log_var = 2.0f;

            if (m->temperature > 0.0f) {
                float std_val = expf(0.5f * log_var);
                raw[d] = mu[d] + std_val * pw_randn() * m->temperature;
            } else {
                raw[d] = mu[d];
            }
        }

        out_energy[t]  = raw[0] * m->norm_std[0] + m->norm_mean[0];
        out_pitch[t]   = raw[1] * m->norm_std[1] + m->norm_mean[1];
        out_voicing[t] = raw[2] * m->norm_std[2] + m->norm_mean[2];
    }

    free(iv);free(pre);free(fwo);free(bwo);free(gh);free(thh);free(nh);free(cat);free(h64);free(h64_lv);
    return 0;
}
