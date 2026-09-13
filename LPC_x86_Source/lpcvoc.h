/*
 * lpcvoc.h
 *
 * Classic LPC source filter vocoder, driven by the same 20 dimensional
 * LPCNet feature vector as nvoc. No weights, no libm in the synthesis
 * path, about 16 multiplies per output sample against FARGAN's 19000.
 * It is a 1970s excitation model and will not sound neural.
 */

#ifndef LPCVOC_H
#define LPCVOC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LPCVOC_FEATURES   20
#define LPCVOC_FRAME      160
#define LPCVOC_SUBFRAME   40
#define LPCVOC_ORDER      16
#define LPCVOC_BANDS      18
#define LPCVOC_SAMPLE_RATE 16000
#define LPCVOC_PF_MAXLAG   288
#define LPCVOC_DISP_MAX    64

typedef struct {
    float    mem[LPCVOC_ORDER];        /* synthesis filter history */
    float    prev_ac[LPCVOC_ORDER + 1];/* last frame autocorrelation */
    float    prev_gain;
    float    deemph;
    float    lp;                       /* excitation low pass state */
    float    hp_mem;                   /* excitation high pass state */
    float    pf_num[LPCVOC_ORDER];     /* postfilter numerator coeffs */
    float    pf_den[LPCVOC_ORDER];     /* postfilter denominator      */
    float    pf_x[LPCVOC_ORDER];
    float    pf_y[LPCVOC_ORDER];
    float    pf_pbuf[LPCVOC_PF_MAXLAG];
    int32_t  pf_pi;
    float    pf_mu;
    float    pf_t;
    float    pf_ei, pf_eo;
    float    tilt;                     /* source tilt low pass state  */
    float    dc_x, dc_y;               /* output high pass state      */
    float    disp_buf[LPCVOC_DISP_MAX];/* pulse dispersion delay line */
    int32_t  disp_i;
    float    nlp;                      /* noise crossover state       */
    float    jit, shim;                /* jitter and shimmer walks    */
    float    phase;                    /* fractional pitch phase      */
    int32_t  pitch_phase;              /* samples until the next pulse */
    int32_t  last_period;
    uint32_t rng;
    int32_t  primed;
} LpcVocState;

void lpcvoc_init(LpcVocState *st);
void lpcvoc_reset(LpcVocState *st);

/* Generate one 10 ms frame, 160 samples of 16 bit PCM. */
void lpcvoc_frame(LpcVocState *st, const float features[LPCVOC_FEATURES],
                  int16_t pcm[LPCVOC_FRAME]);

#ifdef __cplusplus
}
#endif

#endif /* LPCVOC_H */
