/*
 * lpcvoc.c
 *
 * Classic LPC source filter vocoder. Per frame: band cepstrum to band
 * energies to power spectrum to autocorrelation to Levinson Durbin to
 * LPC. Excitation is a pulse train at the predicted pitch mixed with
 * noise by the pitch correlation, through 1/A(z) and de-emphasised.
 * Autocorrelation is interpolated across the four subframes and
 * Levinson rerun for each, which keeps the filter stable.
 */

#include "lpcvoc.h"
#include "lpcvoc_tables.h"
#include "lpcvoc_disp.h"

#define FREQ_SIZE   161
#define WINDOW      320
#define PREEMPH     0.85f
#define NSUB        (LPCVOC_FRAME / LPCVOC_SUBFRAME)

/* One calibration constant, since the band energies arrive unscaled. */
#ifndef LPCVOC_GAIN_SCALE
#define LPCVOC_GAIN_SCALE 1.23e-5f
#endif

#ifndef LPCVOC_VOICE_LO
#define LPCVOC_VOICE_LO (-0.05f)
#endif
#ifndef LPCVOC_VOICE_HI
#define LPCVOC_VOICE_HI (0.28f)
#endif

/* one pole coefficient for the pulse low pass, about 2 kHz at 16 kHz */
#ifndef LPCVOC_EXC_LP
#define LPCVOC_EXC_LP 0.45f
#endif

/*
 * Source shaping. Against a neural vocoder this was 6 dB hot at 6 to
 * 8 kHz and 2 dB light at 300 to 600 Hz. SRC_TILT is a one pole low
 * pass on the excitation, NOISE_HP a 6 dB per octave high pass on the
 * noise, DC_HP a one pole high pass on the output.
 */
#ifndef LPCVOC_SRC_TILT
#define LPCVOC_SRC_TILT 0.20f
#endif
/* undefine to remove the noise high pass entirely, which is the
   setting that matched the neural vocoder's spectral balance best */
/* #define LPCVOC_NOISE_HP 0.20f */
#ifndef LPCVOC_DC_HP
#define LPCVOC_DC_HP 0.962f      /* about 95 Hz, was 60 */
#endif

/* Voiced excitation only, the noise is already spread. 0 off, 1 full. */
#ifndef LPCVOC_NO_DISPERSION
#define LPCVOC_DISPERSION 1.0f
#endif

/*
 * Band split excitation, as in MELP, because real speech is periodic
 * low and noisy high at once. The crossover is the maximum voiced
 * frequency and tracks the frame's voicing. MVF_LO and MVF_HI are the
 * one pole coefficients at zero and full voicing.
 */
#ifndef LPCVOC_BAND_SPLIT
#define LPCVOC_BAND_SPLIT 1
#endif
#ifndef LPCVOC_MVF_LO
#define LPCVOC_MVF_LO 0.92f     /* unvoiced: crossover near 220 Hz  */
#endif
#ifndef LPCVOC_MVF_HI
#define LPCVOC_MVF_HI 0.58f     /* voiced:   crossover near 1.4 kHz */
#endif

/*
 * Jitter and shimmer as random walks scaled by voicing. Both default
 * to 0 because both lost SCOREQ at every setting from 0.3 percent up,
 * even though periodicity moved the right way, 0.7200 to 0.7147
 * against FARGAN's 0.6520.
 */
#ifndef LPCVOC_JITTER
#define LPCVOC_JITTER 0.000f
#endif
#ifndef LPCVOC_SHIMMER
#define LPCVOC_SHIMMER 0.00f
#endif

/*
 * Adaptive postfilter, as in G.729 and AMR. A(z/GN) / A(z/GD) with
 * GN < GD pulls down the valleys between formants, a one zero high
 * pass from the first reflection coefficient puts the tilt back, and a
 * single tap comb at the pitch lag reinforces the harmonics. Gain is
 * renormalised after. About 35 multiplies per sample.
 */
#ifndef LPCVOC_POSTFILTER
#define LPCVOC_POSTFILTER 1
#endif
#ifndef LPCVOC_PF_GN
#define LPCVOC_PF_GN 0.60f      /* numerator bandwidth expansion   */
#endif
#ifndef LPCVOC_PF_GD
#define LPCVOC_PF_GD 0.92f      /* denominator bandwidth expansion */
#endif
#ifndef LPCVOC_PF_TILT
#define LPCVOC_PF_TILT 0.30f    /* tilt compensation strength      */
#endif
#ifndef LPCVOC_PF_PITCH
#define LPCVOC_PF_PITCH 0.00f   /* comb depth, 0 disables          */
#endif

static float nv_sqrt(float x)
{
    /* Newton from the exponent halving trick, no libm */
    union { float f; uint32_t i; } u;
    float y;
    if (x <= 0.0f) return 0.0f;
    u.f = x;
    u.i = 0x1fbd1df5u + (u.i >> 1);
    y = u.f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

static float nv_exp2(float x)
{
    union { float f; uint32_t i; } r;
    int n = (int)(x >= 0.0f ? x : x - 1.0f);
    float f = x - (float)n;
    if (n < -60) return 0.0f;
    if (n > 60) n = 60;
    r.f = 0.99992522f + f * (0.69583354f + f * (0.22606716f + 0.078024523f * f));
    r.i = (uint32_t)((int32_t)r.i + (n << 23));
    return r.f;
}

#define nv_pow10(x) nv_exp2((x) * 3.32192809f)

static uint32_t rnd(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

/* uniform in [-1, 1) */
static float noise(uint32_t *s)
{
    return (float)((int32_t)(rnd(s) >> 8) - 8388608) * (1.0f / 8388608.0f);
}

static void cepstrum_to_ac(const float *ceps, float *ac)
{
    float band[LPCVOC_BANDS];
    float spec[FREQ_SIZE];
    int i, j, k, b;

    /* inverse DCT of the band cepstrum, matching the analysis side */
    for (i = 0; i < LPCVOC_BANDS; i++) {
        float sum = 0.0f;
        for (j = 0; j < LPCVOC_BANDS; j++) {
            float c = (j == 0) ? (ceps[0] + 4.0f) : ceps[j];
            sum += c * lpcvoc_dct[i * LPCVOC_BANDS + j];
        }
        band[i] = nv_pow10(sum * 0.33333333f);
    }

    /* piecewise linear interpolation of band gains onto the bins */
    for (i = 0; i < FREQ_SIZE; i++) spec[i] = 0.0f;
    for (b = 0; b < LPCVOC_BANDS - 1; b++) {
        int lo = lpcvoc_band[b];
        int size = lpcvoc_band[b + 1] - lo;
        for (j = 0; j < size; j++) {
            float frac = (float)j / (float)size;
            spec[lo + j] = (1.0f - frac) * band[b] + frac * band[b + 1];
        }
    }
    spec[FREQ_SIZE - 1] = 0.0f;

    /* real symmetric inverse transform, first ORDER+1 lags only */
    for (k = 0; k <= LPCVOC_ORDER; k++) {
        const float *cs = &lpcvoc_cos[k * FREQ_SIZE];
        float sum = spec[0];
        for (i = 1; i < FREQ_SIZE - 1; i++) sum += 2.0f * spec[i] * cs[i];
        ac[k] = sum;
    }

    /* white noise correction and lag window, as in the analysis path */
    ac[0] += ac[0] * 1e-4f + 1.0f;
    for (k = 1; k <= LPCVOC_ORDER; k++)
        ac[k] *= 1.0f - 6e-5f * (float)(k * k);
}

/* Levinson Durbin. Returns the residual energy. */
static float levinson(const float *ac, float *lpc, int p)
{
    float err = ac[0];
    int i, j;

    for (i = 0; i < p; i++) lpc[i] = 0.0f;
    if (err <= 1e-10f) return 0.0f;

    for (i = 0; i < p; i++) {
        float rr = 0.0f, r;
        for (j = 0; j < i; j++) rr += lpc[j] * ac[i - j];
        rr += ac[i + 1];
        r = -rr / err;
        if (r > 0.999f) r = 0.999f;
        if (r < -0.999f) r = -0.999f;
        lpc[i] = r;
        for (j = 0; j < (i + 1) >> 1; j++) {
            float t1 = lpc[j], t2 = lpc[i - 1 - j];
            lpc[j] = t1 + r * t2;
            lpc[i - 1 - j] = t2 + r * t1;
        }
        err -= r * r * err;
        if (err < 1e-10f) break;
    }

    /* mild bandwidth expansion, keeps the formants from ringing */
    {
        float g = 1.0f;
        for (i = 0; i < p; i++) { g *= 0.995f; lpc[i] *= g; }
    }
    return err;
}

void lpcvoc_init(LpcVocState *st) { lpcvoc_reset(st); }

void lpcvoc_reset(LpcVocState *st)
{
    int i;
    for (i = 0; i < LPCVOC_ORDER; i++) st->mem[i] = 0.0f;
    for (i = 0; i <= LPCVOC_ORDER; i++) st->prev_ac[i] = 0.0f;
    st->prev_ac[0] = 1.0f;
    st->prev_gain = 0.0f;
    st->deemph = 0.0f;
    st->lp = 0.0f;
    st->hp_mem = 0.0f;
    for (i = 0; i < LPCVOC_ORDER; i++) {
        st->pf_num[i] = st->pf_den[i] = st->pf_x[i] = st->pf_y[i] = 0.0f;
    }
    for (i = 0; i < LPCVOC_PF_MAXLAG; i++) st->pf_pbuf[i] = 0.0f;
    st->pf_pi = 0; st->pf_mu = 0.0f; st->pf_t = 0.0f;
    st->pf_ei = st->pf_eo = 1e-9f;
    st->tilt = 0.0f;
    st->dc_x = st->dc_y = 0.0f;
    for (i = 0; i < LPCVOC_DISP_TAPS; i++) st->disp_buf[i] = 0.0f;
    st->disp_i = 0;
    st->nlp = 0.0f;
    st->jit = st->shim = 0.0f;
    st->pitch_phase = 0;
    st->phase = 0.0f;
    st->last_period = 100;
    st->rng = 0x1234567u;
    st->primed = 0;
}

void lpcvoc_frame(LpcVocState *st, const float features[LPCVOC_FEATURES],
                  int16_t pcm[LPCVOC_FRAME])
{
    float ac[LPCVOC_ORDER + 1];
    float lpc[LPCVOC_ORDER];
    float gain;
    int period, sub, i, n;
    float voiced;

    cepstrum_to_ac(features, ac);
    if (!st->primed) {
        for (i = 0; i <= LPCVOC_ORDER; i++) st->prev_ac[i] = ac[i];
        st->primed = 1;
    }

    /* same pitch mapping the neural path uses */
    {
        float p = 256.0f * nv_exp2(-(features[18] + 1.5f));
        period = (int)(p + 0.5f);
        if (period < 32) period = 32;
        if (period > 255) period = 255;
    }

    /* Dim 19 runs about -0.28 to +0.43 here, not 0 to 1. */
    voiced = (features[19] - LPCVOC_VOICE_LO)
           / (LPCVOC_VOICE_HI - LPCVOC_VOICE_LO);
    if (voiced < 0.0f) voiced = 0.0f;
    if (voiced > 1.0f) voiced = 1.0f;

    n = 0;
    for (sub = 0; sub < NSUB; sub++) {
        float sac[LPCVOC_ORDER + 1];
        float w = (float)(sub + 1) / (float)NSUB;
        float err, g0, g1;
        int per;

        for (i = 0; i <= LPCVOC_ORDER; i++)
            sac[i] = (1.0f - w) * st->prev_ac[i] + w * ac[i];

        err = levinson(sac, lpc, LPCVOC_ORDER);
        gain = nv_sqrt(err) * LPCVOC_GAIN_SCALE;

        g0 = (sub == 0) ? st->prev_gain : st->prev_gain;
        g1 = gain;
        (void)g0;

        per = (int)((1.0f - w) * (float)st->last_period + w * (float)period + 0.5f);
        if (per < 32) per = 32;

#if LPCVOC_POSTFILTER
        {
            float g = 1.0f;
            for (i = 0; i < LPCVOC_ORDER; i++) {
                g *= LPCVOC_PF_GN; st->pf_num[i] = lpc[i] * g;
            }
            g = 1.0f;
            for (i = 0; i < LPCVOC_ORDER; i++) {
                g *= LPCVOC_PF_GD; st->pf_den[i] = lpc[i] * g;
            }
            /* first reflection coefficient of the numerator, for tilt */
            {
                float r0 = 1.0f, r1 = st->pf_num[0];
                st->pf_mu = LPCVOC_PF_TILT * (-r1 / (r0 + 1e-9f));
                if (st->pf_mu > 0.5f) st->pf_mu = 0.5f;
                if (st->pf_mu < -0.5f) st->pf_mu = -0.5f;
            }
        }
#endif

        for (i = 0; i < LPCVOC_SUBFRAME; i++, n++) {
            float ex, y;
            int k;

            /* Pulse weighted low, noise weighted high. */
            {
                float pulse = 0.0f, nz;
                if (st->phase <= 0.0f) {
                    float p = (float)per;
                    float amp = 1.0f;

                    /* Fractional: an integer one truncates 1 percent of 80 to nothing. */
                    st->jit = 0.80f * st->jit + 0.20f * noise(&st->rng);
                    if (st->jit > 1.0f) st->jit = 1.0f;
                    if (st->jit < -1.0f) st->jit = -1.0f;
                    p += st->jit * LPCVOC_JITTER * voiced * (float)per;
                    if (p < 20.0f) p = 20.0f;

                    st->shim = 0.80f * st->shim + 0.20f * noise(&st->rng);
                    if (st->shim > 1.0f) st->shim = 1.0f;
                    if (st->shim < -1.0f) st->shim = -1.0f;
                    amp += st->shim * LPCVOC_SHIMMER * voiced;

                    pulse = amp * nv_sqrt((float)per);
                    st->phase += p;
                }
                st->phase -= 1.0f;

#ifdef LPCVOC_DISPERSION
                /* FIR, so the line is mostly zeros and only aligned taps cost. */
                {
                    int d;
                    st->disp_buf[st->disp_i] = pulse;
                    st->disp_i = (st->disp_i + 1) % LPCVOC_DISP_TAPS;
                    {
                        float acc = 0.0f;
                        int idx = st->disp_i;
                        for (d = LPCVOC_DISP_TAPS - 1; d >= 0; d--) {
                            acc += lpcvoc_disp[d] * st->disp_buf[idx];
                            idx = (idx + 1) % LPCVOC_DISP_TAPS;
                        }
                        pulse = (1.0f - LPCVOC_DISPERSION) * pulse
                              + LPCVOC_DISPERSION * acc;
                    }
                }
#endif

#if LPCVOC_BAND_SPLIT
                {
                    /* crossover follows the frame's voicing */
                    float a = LPCVOC_MVF_LO
                            + voiced * (LPCVOC_MVF_HI - LPCVOC_MVF_LO);
                    float nlp;

                    nz = 1.7320508f * noise(&st->rng);

                    /* pulse takes the band below the crossover */
                    st->lp = a * st->lp + (1.0f - a) * pulse;

                    /* noise takes the band above it, complementary so
                       the two sum to a flat source */
                    st->nlp = a * st->nlp + (1.0f - a) * nz;
                    nlp = nz - st->nlp;

                    ex = voiced * (st->lp * (1.0f / (1.0f - a)))
                       + (1.0f - 0.6f * voiced) * nlp;
                }
#else
                /* one pole low pass on the pulse, about 2 kHz */
                st->lp = LPCVOC_EXC_LP * st->lp + (1.0f - LPCVOC_EXC_LP) * pulse;

                nz = 1.7320508f * noise(&st->rng);
#ifdef LPCVOC_NOISE_HP
                {
                    float hp = nz - LPCVOC_NOISE_HP * st->hp_mem;
                    st->hp_mem = nz;
                    nz = hp;
                }
#endif

                ex = voiced * (st->lp * (1.0f / (1.0f - LPCVOC_EXC_LP)))
                   + (1.0f - 0.75f * voiced) * nz;
#endif

#ifdef LPCVOC_SRC_TILT
                /* one pole low pass on the source, the glottal rolloff */
                st->tilt = LPCVOC_SRC_TILT * st->tilt
                         + (1.0f - LPCVOC_SRC_TILT) * ex;
                ex = st->tilt * (1.0f / (1.0f - LPCVOC_SRC_TILT));
#endif
            }
            ex *= g1;

            /* 1/A(z) */
            y = ex;
            for (k = 0; k < LPCVOC_ORDER; k++) y -= lpc[k] * st->mem[k];
            for (k = LPCVOC_ORDER - 1; k > 0; k--) st->mem[k] = st->mem[k - 1];
            st->mem[0] = y;

#if LPCVOC_POSTFILTER
            {
                float in_e = y * y, out_e;
                float w = y;
                int j;

                /* A(z/GN) applied to the synthesis output */
                for (j = 0; j < LPCVOC_ORDER; j++) w += st->pf_num[j] * st->pf_x[j];
                for (j = LPCVOC_ORDER - 1; j > 0; j--) st->pf_x[j] = st->pf_x[j - 1];
                st->pf_x[0] = y;

                /* 1/A(z/GD) */
                for (j = 0; j < LPCVOC_ORDER; j++) w -= st->pf_den[j] * st->pf_y[j];
                for (j = LPCVOC_ORDER - 1; j > 0; j--) st->pf_y[j] = st->pf_y[j - 1];
                st->pf_y[0] = w;

                /* tilt compensation, one zero */
                {
                    float t = w - st->pf_mu * st->pf_t;
                    st->pf_t = w;
                    w = t;
                }

                /* single tap pitch comb, only where the frame is voiced */
                if (LPCVOC_PF_PITCH > 0.0f && voiced > 0.4f) {
                    int lag = per;
                    if (lag > 0 && lag < LPCVOC_PF_MAXLAG) {
                        float p = st->pf_pbuf[(st->pf_pi - lag + LPCVOC_PF_MAXLAG)
                                              % LPCVOC_PF_MAXLAG];
                        float a = LPCVOC_PF_PITCH * voiced;
                        w = (w + a * p) / (1.0f + a);
                    }
                    st->pf_pbuf[st->pf_pi] = w;
                    st->pf_pi = (st->pf_pi + 1) % LPCVOC_PF_MAXLAG;
                } else {
                    st->pf_pbuf[st->pf_pi] = w;
                    st->pf_pi = (st->pf_pi + 1) % LPCVOC_PF_MAXLAG;
                }

                /* gain match, one pole tracker so the filter is level neutral */
                out_e = w * w;
                st->pf_ei = 0.95f * st->pf_ei + 0.05f * in_e;
                st->pf_eo = 0.95f * st->pf_eo + 0.05f * out_e;
                if (st->pf_eo > 1e-12f)
                    w *= nv_sqrt(st->pf_ei / st->pf_eo);
                y = w;
            }
#endif

            /* de-emphasis, matching the 0.85 pre-emphasis of the analysis */
            y += PREEMPH * st->deemph;
            st->deemph = y;

#ifdef LPCVOC_DC_HP
            /* one pole high pass, removes sub voice band rumble */
            {
                float o = y - st->dc_x + LPCVOC_DC_HP * st->dc_y;
                st->dc_x = y;
                st->dc_y = o;
                y = o;
            }
#endif

            {
                float s = y * 32768.0f;
                int32_t v = (int32_t)(s >= 0.0f ? s + 0.5f : s - 0.5f);
                if (v > 32767) v = 32767;
                if (v < -32767) v = -32767;
                pcm[n] = (int16_t)v;
            }
        }
        st->prev_gain = g1;
    }

    for (i = 0; i <= LPCVOC_ORDER; i++) st->prev_ac[i] = ac[i];
    st->last_period = period;
}
