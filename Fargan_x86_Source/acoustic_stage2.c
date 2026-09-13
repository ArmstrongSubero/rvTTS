/*
 * acoustic_stage2.c
 *
 * Stage 2 acoustic model, int8 weights against float32 activations.
 */

#include "acoustic_stage2.h"
#include "acoustic_stage2_weights_int8_meta.h"
#include "phonemes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int as2_is_vowel(uint8_t ph) {
    switch (ph) { case PH_AA: case PH_AE: case PH_AH: case PH_AO: case PH_AW: case PH_AY: case PH_EH: case PH_ER: case PH_EY: case PH_IH: case PH_IY: case PH_OW: case PH_OY: case PH_UH: case PH_UW: return 1; default: return 0; }
}
static int as2_is_fricative(uint8_t ph) {
    switch (ph) { case PH_F: case PH_V: case PH_S: case PH_Z: case PH_SH: case PH_ZH: case PH_TH: case PH_DH: case PH_HH: return 1; default: return 0; }
}
static int as2_is_stop(uint8_t ph) {
    switch (ph) { case PH_B: case PH_D: case PH_G: case PH_P: case PH_T: case PH_K: return 1; default: return 0; }
}
static int as2_is_affricate(uint8_t ph) { return ph == PH_CH || ph == PH_JH; }
static int as2_is_nasal(uint8_t ph) { return ph == PH_M || ph == PH_N || ph == PH_NG; }
static int as2_is_liquid(uint8_t ph) { return ph == PH_L || ph == PH_R; }
static int as2_is_glide(uint8_t ph) { return ph == PH_W || ph == PH_Y; }
static int as2_is_voiced(uint8_t ph) {
    return as2_is_vowel(ph) || as2_is_nasal(ph) || as2_is_liquid(ph) || as2_is_glide(ph) || ph == PH_B || ph == PH_D || ph == PH_G || ph == PH_V || ph == PH_Z || ph == PH_ZH || ph == PH_DH || ph == PH_JH;
}
static int as2_is_sonorant(uint8_t ph) { return as2_is_vowel(ph) || as2_is_nasal(ph) || as2_is_liquid(ph) || as2_is_glide(ph); }
static int as2_is_silence(uint8_t ph) { return ph == PH_SIL || ph == PH_BRK; }

static uint8_t as2_prev_phone(const uint8_t *fp, uint32_t t) {
    if (t == 0) return PH_SIL; uint8_t c = fp[t]; uint32_t i = t;
    while (i > 0 && fp[i-1] == c) i--; return (i == 0) ? PH_SIL : fp[i-1];
}
static uint8_t as2_next_phone(const uint8_t *fp, uint32_t t, uint32_t n) {
    uint8_t c = fp[t]; uint32_t i = t;
    while (i+1 < n && fp[i+1] == c) i++; return (i+1 >= n) ? PH_SIL : fp[i+1];
}
static uint32_t as2_seg_start(const uint8_t *fp, uint32_t t) {
    uint8_t ph = fp[t]; uint32_t s = t; while (s > 0 && fp[s-1] == ph) s--; return s;
}
static uint32_t as2_seg_end(const uint8_t *fp, uint32_t t, uint32_t n) {
    uint8_t ph = fp[t]; uint32_t e = t+1; while (e < n && fp[e] == ph) e++; return e;
}

static void as2_linear_i8(const int8_t *w, const float *b, float scale,
                           const float *x, uint32_t od, uint32_t id, float *y)
{
    for (uint32_t o = 0; o < od; o++) {
        const int8_t *row = w + (size_t)o * id;
        float acc = 0.0f;
        for (uint32_t i = 0; i < id; i++) acc += (float)row[i] * x[i];
        y[o] = acc * scale + (b ? b[o] : 0.0f);
    }
}

static void as2_emb_i8(const int8_t *e, float s, uint32_t id, uint32_t d, float *o)
{ const int8_t *r = e + (size_t)id * d; for (uint32_t i = 0; i < d; i++) o[i] = (float)r[i] * s; }

int acoustic_stage2_load(AcousticStage2 *m, const char *weights_path)
{
    memset(m, 0, sizeof(*m));
    FILE *f = fopen(weights_path, "rb");
    if (!f) { fprintf(stderr, "Cannot open stage2 weights: %s\n", weights_path); return -1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    m->blob = (uint8_t *)malloc((size_t)sz);
    if (!m->blob) { fclose(f); return -1; }
    fread(m->blob, 1, (size_t)sz, f); fclose(f);
    m->blob_size = (size_t)sz;

    uint32_t magic; memcpy(&magic, m->blob, 4);

    if (magic != AS2Q_WEIGHT_MAGIC) {
        fprintf(stderr, "Expected int8 acoustic weights (magic 0x%08X), got 0x%08X\n",
                AS2Q_WEIGHT_MAGIC, magic);
        free(m->blob); memset(m, 0, sizeof(*m)); return -1;
    }

    m->phone_emb_i8      = (const int8_t *)(m->blob + AS2Q_OFF_PHONE_EMB);
    m->stress_emb_i8     = (const int8_t *)(m->blob + AS2Q_OFF_STRESS_EMB);
    m->conv_pre_w_i8     = (const int8_t *)(m->blob + AS2Q_OFF_CONV_PRE_W);
    m->conv_pre_b        = (const float *)(m->blob + AS2Q_OFF_CONV_PRE_B);
    m->context_conv_w_i8 = (const int8_t *)(m->blob + AS2Q_OFF_CONTEXT_CONV_W);
    m->dilated_w_i8[0] = (const int8_t *)(m->blob + AS2Q_OFF_DILATED_0_W); m->dilated_b[0] = (const float *)(m->blob + AS2Q_OFF_DILATED_0_B);
    m->dilated_w_i8[1] = (const int8_t *)(m->blob + AS2Q_OFF_DILATED_1_W); m->dilated_b[1] = (const float *)(m->blob + AS2Q_OFF_DILATED_1_B);
    m->dilated_w_i8[2] = (const int8_t *)(m->blob + AS2Q_OFF_DILATED_2_W); m->dilated_b[2] = (const float *)(m->blob + AS2Q_OFF_DILATED_2_B);
    m->dilated_w_i8[3] = (const int8_t *)(m->blob + AS2Q_OFF_DILATED_3_W); m->dilated_b[3] = (const float *)(m->blob + AS2Q_OFF_DILATED_3_B);
    m->dilated_w_i8[4] = (const int8_t *)(m->blob + AS2Q_OFF_DILATED_4_W); m->dilated_b[4] = (const float *)(m->blob + AS2Q_OFF_DILATED_4_B);
    m->out_w1_i8 = (const int8_t *)(m->blob + AS2Q_OFF_OUT_W1); m->out_b1 = (const float *)(m->blob + AS2Q_OFF_OUT_B1);
    m->out_w2_i8 = (const int8_t *)(m->blob + AS2Q_OFF_OUT_W2); m->out_b2 = (const float *)(m->blob + AS2Q_OFF_OUT_B2);
    m->norm_mean = (const float *)(m->blob + AS2Q_OFF_NORM_MEAN);
    m->norm_std  = (const float *)(m->blob + AS2Q_OFF_NORM_STD);
    m->scale_phone_emb = AS2Q_SCALE_PHONE_EMB; m->scale_stress_emb = AS2Q_SCALE_STRESS_EMB;
    m->scale_conv_pre_w = AS2Q_SCALE_CONV_PRE_W; m->scale_context_conv_w = AS2Q_SCALE_CONTEXT_CONV_W;
    m->scale_dilated_w[0] = AS2Q_SCALE_DILATED_0_W; m->scale_dilated_w[1] = AS2Q_SCALE_DILATED_1_W;
    m->scale_dilated_w[2] = AS2Q_SCALE_DILATED_2_W; m->scale_dilated_w[3] = AS2Q_SCALE_DILATED_3_W;
    m->scale_dilated_w[4] = AS2Q_SCALE_DILATED_4_W;
    m->scale_out_w1 = AS2Q_SCALE_OUT_W1; m->scale_out_w2 = AS2Q_SCALE_OUT_W2;
    printf("Loaded acoustic stage2 (int8, %zu bytes)\n", m->blob_size);
    return 0;
}

void acoustic_stage2_free(AcousticStage2 *m)
{
    if (m && m->blob) free(m->blob);
    if (m) memset(m, 0, sizeof(*m));
}

int acoustic_stage2_generate(
    AcousticStage2 *m, const uint8_t *frame_phones, const uint8_t *frame_stress,
    const float *cond_energy, const float *cond_pitch, const float *cond_voicing,
    uint32_t frame_count, float *out_features)
{
    if (!m || !m->blob || !frame_phones || frame_count == 0) return -1;
    const uint32_t C=AS2_CONV_CHANNELS, IN=AS2_INPUT_DIM, CK=AS2_CONV_KERNEL, CP=AS2_CONV_PAD;
    const uint32_t ND=AS2_N_DILATED, DK=AS2_DILATED_KERNEL, PRED=AS2_PRED_DIM;
    static const int ci[3]={0,18,19};
    static const int pi[17]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17};

    float *iv=(float*)malloc(IN*sizeof(float));
    float *cin=(float*)malloc((size_t)frame_count*C*sizeof(float));
    float *cout=(float*)malloc((size_t)frame_count*C*sizeof(float));
    float *ctmp=(float*)malloc((size_t)frame_count*C*sizeof(float));
    float *h128=(float*)malloc(C*sizeof(float));
    if(!iv||!cin||!cout||!ctmp||!h128){free(iv);free(cin);free(cout);free(ctmp);free(h128);return -1;}

    for(uint32_t t=0;t<frame_count;t++){
        uint32_t p=0;
        uint8_t cp=frame_phones[t],pp=as2_prev_phone(frame_phones,t),np=as2_next_phone(frame_phones,t,frame_count);
        if(cp>=AS2_PHONE_COUNT)cp=0;if(pp>=AS2_PHONE_COUNT)pp=0;if(np>=AS2_PHONE_COUNT)np=0;

        as2_emb_i8(m->phone_emb_i8,m->scale_phone_emb,pp,AS2_PHONE_EMB_DIM,iv+p);p+=AS2_PHONE_EMB_DIM;
        as2_emb_i8(m->phone_emb_i8,m->scale_phone_emb,cp,AS2_PHONE_EMB_DIM,iv+p);p+=AS2_PHONE_EMB_DIM;
        as2_emb_i8(m->phone_emb_i8,m->scale_phone_emb,np,AS2_PHONE_EMB_DIM,iv+p);p+=AS2_PHONE_EMB_DIM;
        uint8_t s=frame_stress?frame_stress[t]:0;if(s>=AS2_STRESS_COUNT)s=0;
        as2_emb_i8(m->stress_emb_i8,m->scale_stress_emb,s,AS2_STRESS_EMB_DIM,iv+p);p+=AS2_STRESS_EMB_DIM;

        iv[p++]=(float)as2_is_vowel(cp);iv[p++]=(float)as2_is_fricative(cp);iv[p++]=(float)as2_is_stop(cp);
        iv[p++]=(float)as2_is_affricate(cp);iv[p++]=(float)as2_is_nasal(cp);iv[p++]=(float)as2_is_liquid(cp);
        iv[p++]=(float)as2_is_glide(cp);iv[p++]=(float)as2_is_sonorant(cp);iv[p++]=(float)as2_is_voiced(cp);
        iv[p++]=(float)as2_is_silence(cp);
        uint32_t ss=as2_seg_start(frame_phones,t),se=as2_seg_end(frame_phones,t,frame_count),sl=se-ss;
        float pos=(sl>1)?(float)(t-ss)/(float)(sl-1):0.0f;
        iv[p++]=pos;iv[p++]=1.0f-pos;
        float ds=(float)sl/30.0f;if(ds>2.0f)ds=2.0f;
        iv[p++]=ds;iv[p++]=logf(1.0f+(float)sl)/5.0f;
        iv[p++]=(cond_energy[t]-m->norm_mean[ci[0]])/m->norm_std[ci[0]];
        iv[p++]=(cond_pitch[t]-m->norm_mean[ci[1]])/m->norm_std[ci[1]];
        iv[p++]=(cond_voicing[t]-m->norm_mean[ci[2]])/m->norm_std[ci[2]];

        as2_linear_i8(m->conv_pre_w_i8,m->conv_pre_b,m->scale_conv_pre_w,iv,C,IN,cin+(size_t)t*C);
        float*cc=cin+(size_t)t*C; for(uint32_t j=0;j<C;j++)cc[j]=tanhf(cc[j]);
    }

    for(uint32_t t=0;t<frame_count;t++){
        float*co=cout+(size_t)t*C; memset(co,0,C*sizeof(float));
        for(uint32_t k=0;k<CK;k++){
            int32_t st=(int32_t)t-(int32_t)CP+(int32_t)k;
            if(st<0||st>=(int32_t)frame_count)continue;
            const float*src=cin+(size_t)st*C;
            for(uint32_t o=0;o<C;o++){
                float a=0.0f;
                const int8_t*wk=m->context_conv_w_i8+(size_t)o*C*CK+k;
                for(uint32_t i=0;i<C;i++)a+=(float)wk[(size_t)i*CK]*src[i];
                co[o]+=a*m->scale_context_conv_w;
            }
        }
        for(uint32_t j=0;j<C;j++)co[j]=tanhf(co[j]);
    }

    for(uint32_t l=0;l<ND;l++){
        uint32_t dil=1u<<l,pad=dil;
        for(uint32_t t=0;t<frame_count;t++){
            float*out=ctmp+(size_t)t*C;
            for(uint32_t o=0;o<C;o++){
                float a=m->dilated_b[l][o];
                for(uint32_t k=0;k<DK;k++){
                    int32_t st=(int32_t)t-(int32_t)pad+(int32_t)(k*dil);
                    if(st<0||st>=(int32_t)frame_count)continue;
                    const float*src=cout+(size_t)st*C;
                    const int8_t*wk=m->dilated_w_i8[l]+(size_t)o*C*DK+(size_t)k;
                    float wa=0;for(uint32_t i=0;i<C;i++)wa+=(float)wk[(size_t)i*DK]*src[i];
                    a+=wa*m->scale_dilated_w[l];
                }
                out[o]=tanhf(a);
            }
            const float*res=cout+(size_t)t*C;for(uint32_t j=0;j<C;j++)out[j]+=res[j];
        }
        float*sw=cout;cout=ctmp;ctmp=sw;
    }

    for(uint32_t t=0;t<frame_count;t++){
        as2_linear_i8(m->out_w1_i8,m->out_b1,m->scale_out_w1,cout+(size_t)t*C,C,C,h128);
        for(uint32_t j=0;j<C;j++)h128[j]=tanhf(h128[j]);
        float raw[17];
        as2_linear_i8(m->out_w2_i8,m->out_b2,m->scale_out_w2,h128,PRED,C,raw);
        float*feat=out_features+(size_t)t*20;
        for(uint32_t d=0;d<PRED;d++){int dim=pi[d];feat[dim]=raw[d]*m->norm_std[dim]+m->norm_mean[dim];}
    }

    free(iv);free(cin);free(cout);free(ctmp);free(h128);
    return 0;
}
