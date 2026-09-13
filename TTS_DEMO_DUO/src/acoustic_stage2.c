/*
 * acoustic_stage2.c
 *
 * Stage 2 acoustic model, int8 weights against float32 activations.
 */

#include "acoustic_stage2.h"
#include "acoustic_stage2_weights_int8_meta.h"
#include "phonemes.h"

#include "tts_compat.h"

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

/* Bind a weight blob already in memory. See prosody_load_mem. */
int acoustic_stage2_load_mem(AcousticStage2 *m, const void *blob, uint32_t len)
{
    memset(m, 0, sizeof(*m));
    if (!blob || len < 16) return -1;
    m->blob = (uint8_t *)(uintptr_t)blob;
    m->blob_size = (size_t)len;

    uint32_t magic; memcpy(&magic, m->blob, 4);

    if (magic != AS2Q_WEIGHT_MAGIC) {
        memset(m, 0, sizeof(*m)); return -1;
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
    return 0;
}

#ifdef TTS_HOSTED
#include <stdio.h>
#include <stdlib.h>

int acoustic_stage2_load(AcousticStage2 *m, const char *weights_path)
{
    FILE *f = fopen(weights_path, "rb");
    long sz; uint8_t *buf; int r;
    if (!f) { fprintf(stderr, "Cannot open acoustic weights: %s\n", weights_path); return -1; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(buf); return -1; }
    fclose(f);
    r = acoustic_stage2_load_mem(m, buf, (uint32_t)sz);
    if (r != 0) { free(buf); return r; }
    m->owns_blob = 1;
    TTS_LOG("Loaded acoustic stage2 (int8, %ld bytes)\n", sz);
    return 0;
}
#endif /* TTS_HOSTED */

static void *g_as2_scratch;
static size_t g_as2_scratch_bytes;

/* Bytes of scratch acoustic_stage2_generate needs for frame_count frames. */
size_t acoustic_stage2_scratch_bytes(uint32_t frame_count)
{
    return (size_t)(AS2_INPUT_DIM + AS2_CONV_CHANNELS) * sizeof(float)
         + (size_t)frame_count * AS2_CONV_CHANNELS * 3u * sizeof(float);
}

/* Hand the module its working memory. Call once before generating. */
void acoustic_stage2_set_scratch(void *mem, size_t bytes)
{
    g_as2_scratch = mem;
    g_as2_scratch_bytes = bytes;
}

void acoustic_stage2_free(AcousticStage2 *m)
{
#ifdef TTS_HOSTED
    if (m && m->owns_blob && m->blob) free(m->blob);
#endif
    if (m) memset(m, 0, sizeof(*m));
}

/* One [o][i] tap staged into RAM transposed. Same order, bit identical. */

static uint8_t *g_as2_stage;
static size_t   g_as2_stage_bytes;

void acoustic_stage2_set_stage_buf(void *buf, size_t bytes)
{
    g_as2_stage = (uint8_t *)buf;
    g_as2_stage_bytes = bytes;
}

/* Copy one [out][in][K] tap into a contiguous slice. NULL if no buffer. */
static const int8_t *as2_stage_tap(const int8_t *w, uint32_t k, uint32_t K,
                                   uint32_t od, uint32_t id)
{
    size_t need = (size_t)od * id;
    int8_t *dst;
    uint32_t o, i;

    if (!g_as2_stage || g_as2_stage_bytes < need) return NULL;

    dst = (int8_t *)g_as2_stage;
    for (o = 0; o < od; o++) {
        const int8_t *src = w + (size_t)o * id * K + k;
        int8_t *row = dst + (size_t)o * id;
        for (i = 0; i < id; i++) row[i] = src[(size_t)i * K];
    }
    return dst;
}

/* Copy a plain [out][in] block, already contiguous, into RAM. */
static const int8_t *as2_stage_block(const int8_t *w, size_t bytes, size_t off)
{
    if (!g_as2_stage || g_as2_stage_bytes < off + bytes) return NULL;
    memcpy(g_as2_stage + off, w, bytes);
    return (const int8_t *)(g_as2_stage + off);
}

/* -DAS2_INT8_ACT=0 keeps the float accumulator, the numerical reference. */

#ifndef AS2_INT8_ACT
#define AS2_INT8_ACT 1
#endif

#if AS2_INT8_ACT

#define AS2_QMAX ((AS2_INPUT_DIM > AS2_CONV_CHANNELS) \
                  ? AS2_INPUT_DIM : AS2_CONV_CHANNELS)

static int8_t g_as2_q[AS2_QMAX];

/* One symmetric scale, folded into the weight scale after the dot. */
static float as2_quant(const float *x, uint32_t n, int8_t *q)
{
    float m = 0.0f, inv;
    uint32_t i;

    for (i = 0; i < n; i++) {
        float v = (x[i] < 0.0f) ? -x[i] : x[i];
        if (v > m) m = v;
    }
    if (m <= 0.0f) {
        for (i = 0; i < n; i++) q[i] = 0;
        return 0.0f;
    }

    inv = 127.0f / m;
    for (i = 0; i < n; i++) {
        float v = x[i] * inv;
        int32_t r = (int32_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
        if (r >  127) r =  127;
        if (r < -127) r = -127;
        q[i] = (int8_t)r;
    }
    return m / 127.0f;
}

/* 128 terms of 127 by 127 peaks at 2064512, and 169 terms at 2725801,
   so int32 cannot overflow here and needs no saturation. */
static int32_t as2_dot_i8(const int8_t *w, const int8_t *x, uint32_t n)
{
    int32_t a = 0;
    uint32_t i = 0;

    for (; i + 4u <= n; i += 4u) {
        a += (int32_t)w[i]     * (int32_t)x[i];
        a += (int32_t)w[i + 1] * (int32_t)x[i + 1];
        a += (int32_t)w[i + 2] * (int32_t)x[i + 2];
        a += (int32_t)w[i + 3] * (int32_t)x[i + 3];
    }
    for (; i < n; i++) a += (int32_t)w[i] * (int32_t)x[i];
    return a;
}

/* Contiguous [out][in] weights, quantising the activation once. */
static void as2_linear_i8q(const int8_t *w, const float *b, float scale,
                           const float *x, uint32_t od, uint32_t id, float *y)
{
    float s = as2_quant(x, id, g_as2_q);
    uint32_t o;

    for (o = 0; o < od; o++) {
        int32_t a = as2_dot_i8(w + (size_t)o * id, g_as2_q, id);
        y[o] = (float)a * s * scale + (b ? b[o] : 0.0f);
    }
}

#define AS2_LINEAR as2_linear_i8q
#else
#define AS2_LINEAR as2_linear_i8
#endif

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

    /* Caller owned arena, sized by acoustic_stage2_scratch_bytes(). */
    float *iv, *cin, *cout, *ctmp, *h128;
    {
        uint8_t *p = (uint8_t *)g_as2_scratch;
        size_t need = acoustic_stage2_scratch_bytes(frame_count);
        if (!p || g_as2_scratch_bytes < need) return -2;
        iv   = (float *)p; p += IN * sizeof(float);
        cin  = (float *)p; p += (size_t)frame_count * C * sizeof(float);
        cout = (float *)p; p += (size_t)frame_count * C * sizeof(float);
        ctmp = (float *)p; p += (size_t)frame_count * C * sizeof(float);
        h128 = (float *)p;
    }

    /* conv_pre is contiguous already, but every frame re-reads all
       21632 bytes of it; staged, that becomes one read per window. */
    const int8_t *w_pre = as2_stage_block(m->conv_pre_w_i8,(size_t)C*IN,0);
    if(!w_pre) w_pre = m->conv_pre_w_i8;

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
        iv[p++]=ds;iv[p++]=tts_logf(1.0f+(float)sl)/5.0f;
        iv[p++]=(cond_energy[t]-m->norm_mean[ci[0]])/m->norm_std[ci[0]];
        iv[p++]=(cond_pitch[t]-m->norm_mean[ci[1]])/m->norm_std[ci[1]];
        iv[p++]=(cond_voicing[t]-m->norm_mean[ci[2]])/m->norm_std[ci[2]];

        AS2_LINEAR(w_pre,m->conv_pre_b,m->scale_conv_pre_w,iv,C,IN,cin+(size_t)t*C);
        float*cc=cin+(size_t)t*C; for(uint32_t j=0;j<C;j++)cc[j]=tts_tanhf(cc[j]);
    }

    /* k hoisted outside t, still ascending, so the sum order is unchanged. */
    for(uint32_t t=0;t<frame_count;t++)memset(cout+(size_t)t*C,0,C*sizeof(float));
    for(uint32_t k=0;k<CK;k++){
        const int8_t*wt=as2_stage_tap(m->context_conv_w_i8,k,CK,C,C);
        for(uint32_t t=0;t<frame_count;t++){
            int32_t st=(int32_t)t-(int32_t)CP+(int32_t)k;
            if(st<0||st>=(int32_t)frame_count)continue;
            const float*src=cin+(size_t)st*C;
            float*co=cout+(size_t)t*C;
#if AS2_INT8_ACT
            /* one scale for all 128 outputs at this frame and tap */
            float s_act = wt ? as2_quant(src,C,g_as2_q) : 0.0f;
#endif
            for(uint32_t o=0;o<C;o++){
                float a=0.0f;
                if(wt){
#if AS2_INT8_ACT
                    a=(float)as2_dot_i8(wt+(size_t)o*C,g_as2_q,C)*s_act;
#else
                    const int8_t*row=wt+(size_t)o*C;
                    for(uint32_t i=0;i<C;i++)a+=(float)row[i]*src[i];
#endif
                }else{
                    const int8_t*wk=m->context_conv_w_i8+(size_t)o*C*CK+k;
                    for(uint32_t i=0;i<C;i++)a+=(float)wk[(size_t)i*CK]*src[i];
                }
                co[o]+=a*m->scale_context_conv_w;
            }
        }
    }
    for(uint32_t t=0;t<frame_count;t++){
        float*co=cout+(size_t)t*C;
        for(uint32_t j=0;j<C;j++)co[j]=tts_tanhf(co[j]);
    }

    /* Same hoist, ctmp carries the running sum. Bit identical. */
    for(uint32_t l=0;l<ND;l++){
        uint32_t dil=1u<<l,pad=dil;

        for(uint32_t t=0;t<frame_count;t++){
            float*out=ctmp+(size_t)t*C;
            for(uint32_t o=0;o<C;o++)out[o]=m->dilated_b[l][o];
        }

        for(uint32_t k=0;k<DK;k++){
            const int8_t*wt=as2_stage_tap(m->dilated_w_i8[l],k,DK,C,C);
            for(uint32_t t=0;t<frame_count;t++){
                int32_t st=(int32_t)t-(int32_t)pad+(int32_t)(k*dil);
                if(st<0||st>=(int32_t)frame_count)continue;
                const float*src=cout+(size_t)st*C;
                float*out=ctmp+(size_t)t*C;
#if AS2_INT8_ACT
                float s_act = wt ? as2_quant(src,C,g_as2_q) : 0.0f;
#endif
                for(uint32_t o=0;o<C;o++){
                    float wa=0;
                    if(wt){
#if AS2_INT8_ACT
                        wa=(float)as2_dot_i8(wt+(size_t)o*C,g_as2_q,C)*s_act;
#else
                        const int8_t*row=wt+(size_t)o*C;
                        for(uint32_t i=0;i<C;i++)wa+=(float)row[i]*src[i];
#endif
                    }else{
                        const int8_t*wk=m->dilated_w_i8[l]+(size_t)o*C*DK+(size_t)k;
                        for(uint32_t i=0;i<C;i++)wa+=(float)wk[(size_t)i*DK]*src[i];
                    }
                    out[o]+=wa*m->scale_dilated_w[l];
                }
            }
        }

        for(uint32_t t=0;t<frame_count;t++){
            float*out=ctmp+(size_t)t*C;
            const float*res=cout+(size_t)t*C;
            for(uint32_t o=0;o<C;o++)out[o]=tts_tanhf(out[o]);
            for(uint32_t j=0;j<C;j++)out[j]+=res[j];
        }

        float*sw=cout;cout=ctmp;ctmp=sw;
    }

    /* both head matrices fit side by side, staged once per window */
    const int8_t *w1=as2_stage_block(m->out_w1_i8,(size_t)C*C,0);
    const int8_t *w2=w1?as2_stage_block(m->out_w2_i8,(size_t)PRED*C,(size_t)C*C):NULL;
    if(!w1) w1=m->out_w1_i8;
    if(!w2) w2=m->out_w2_i8;

    for(uint32_t t=0;t<frame_count;t++){
        AS2_LINEAR(w1,m->out_b1,m->scale_out_w1,cout+(size_t)t*C,C,C,h128);
        for(uint32_t j=0;j<C;j++)h128[j]=tts_tanhf(h128[j]);
        float raw[17];
        AS2_LINEAR(w2,m->out_b2,m->scale_out_w2,h128,PRED,C,raw);
        float*feat=out_features+(size_t)t*20;
        for(uint32_t d=0;d<PRED;d++){int dim=pi[d];feat[dim]=raw[d]*m->norm_std[dim]+m->norm_mean[dim];}
    }

    return 0;
}

/*
 * 376 KB of arena at 250 frames does not fit 256 KB DTCM. The
 * receptive field is plus or minus 34 frames, so AS2_CHUNK output
 * frames with AS2_PAD of context each side is bit identical at
 * AS2_PAD >= 34. AS2_PAD is 31 to keep the arena at 214 KB. Cost 1.775x.
 */

#ifndef AS2_CHUNK
#define AS2_CHUNK 80u
#endif
#ifndef AS2_PAD
#define AS2_PAD   34u
#endif

#define AS2_WINDOW (AS2_CHUNK + 2u * AS2_PAD)

/* Bytes of scratch acoustic_stage2_generate_chunked needs, regardless
   of utterance length. */
size_t acoustic_stage2_chunk_scratch_bytes(void)
{
    return acoustic_stage2_scratch_bytes(AS2_WINDOW);
}

/* Frames in one window, for sizing the caller's window buffers. */
uint32_t acoustic_stage2_window_frames(void)
{
    return AS2_WINDOW;
}

/* Same contract, arena sized for one window. win_* are AS2_WINDOW entries. */
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
) {
    uint32_t start;

    if (frame_count == 0) return 0;

    for (start = 0; start < frame_count; start += AS2_CHUNK) {
        uint32_t keep = frame_count - start;
        uint32_t lo, hi, wlen, off, i;

        if (keep > AS2_CHUNK) keep = AS2_CHUNK;

        /* window covers [lo, hi), clipped to the utterance */
        lo = (start > AS2_PAD) ? (start - AS2_PAD) : 0u;
        hi = start + keep + AS2_PAD;
        if (hi > frame_count) hi = frame_count;
        wlen = hi - lo;
        off = start - lo;          /* where the kept frames sit in the window */

        for (i = 0; i < wlen; i++) {
            win_phones[i]  = frame_phones[lo + i];
            win_stress[i]  = frame_stress[lo + i];
            win_energy[i]  = cond_energy[lo + i];
            win_pitch[i]   = cond_pitch[lo + i];
            win_voicing[i] = cond_voicing[lo + i];
        }

        if (acoustic_stage2_generate(m, win_phones, win_stress,
                                     win_energy, win_pitch, win_voicing,
                                     wlen, win_out) != 0) {
            return -1;
        }

        /* Stride 20, 17 dims predicted; the caller overwrites 0, 18 and 19. */
        for (i = 0; i < keep * 20u; i++) {
            out_features[(size_t)start * 20u + i] =
                win_out[(size_t)off * 20u + i];
        }
    }
    return 0;
}