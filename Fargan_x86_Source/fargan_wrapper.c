#include "fargan_wrapper.h"

/* From the Opus dnn tree. */
#include "fargan.h"
#include "lpcnet.h"
#include "freq.h"
#include "arch.h"
#include "os_support.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NB_FEATURES
#define NB_FEATURES 20
#endif

#ifdef FARGAN_USE_INT8
static void fargan_force_int8(FARGANState *st)
{
    FARGAN *m = &st->model;

    m->cond_net_fconv1.float_weights = NULL;
    m->cond_net_fdense2.float_weights = NULL;

    m->sig_net_fwc0_conv.float_weights = NULL;
    m->sig_net_fwc0_glu_gate.float_weights = NULL;

    m->sig_net_gru1_input.float_weights = NULL;
    m->sig_net_gru1_recurrent.float_weights = NULL;
    m->sig_net_gru1_glu_gate.float_weights = NULL;

    m->sig_net_gru2_input.float_weights = NULL;
    m->sig_net_gru2_recurrent.float_weights = NULL;
    m->sig_net_gru2_glu_gate.float_weights = NULL;

    m->sig_net_gru3_input.float_weights = NULL;
    m->sig_net_gru3_recurrent.float_weights = NULL;
    m->sig_net_gru3_glu_gate.float_weights = NULL;

    m->sig_net_skip_dense.float_weights = NULL;
    m->sig_net_skip_glu_gate.float_weights = NULL;
    m->sig_net_sig_dense_out.float_weights = NULL;

    printf("[FARGAN] Int8 inference enabled (16 layers switched)\n");
}
#endif

/* Shift pitch onto the speaker mean, LJSpeech: mean 0.2120, std 0.3615. */
#define PITCH_CORRECTION_ENABLED

#ifdef PITCH_CORRECTION_ENABLED
#define SPEAKER_PITCH_MEAN   0.2120f
#define SPEAKER_PITCH_STD    0.3615f

static void correct_pitch(float *features, uint32_t frame_count)
{
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t i = 0; i < frame_count; i++) {
        float p = features[i * NB_FEATURES + NB_BANDS];
        sum += p;
        sum_sq += p * p;
    }

    float pred_mean = sum / (float)frame_count;
    float pred_var = sum_sq / (float)frame_count - pred_mean * pred_mean;
    float pred_std = (pred_var > 1e-10f) ? sqrtf(pred_var) : 1e-5f;

    for (uint32_t i = 0; i < frame_count; i++) {
        float *p = &features[i * NB_FEATURES + NB_BANDS];
        // more robotic but clearer
        //*p = *p - pred_mean + SPEAKER_PITCH_MEAN;

        // more natural variation but some glitches
        //*p = (*p - pred_mean) / pred_std * SPEAKER_PITCH_STD + SPEAKER_PITCH_MEAN;

        float normalized = (*p - pred_mean) / pred_std;
        if (normalized > 2.5f) normalized = 2.5f;
        if (normalized < -2.5f) normalized = -2.5f;
        *p = normalized * SPEAKER_PITCH_STD + SPEAKER_PITCH_MEAN;
    }
}
#endif

static int16_t float_to_pcm16(float x)
{
    float y = 32768.0f * x;

    if (y > 32767.0f) {
        y = 32767.0f;
    } else if (y < -32767.0f) {
        y = -32767.0f;
    }

    return (int16_t)floorf(y + 0.5f);
}

int tts_fargan_synthesize_pcm16(
    const float *features,
    uint32_t frame_count,
    int16_t *out_pcm,
    uint32_t max_samples,
    uint32_t *out_sample_count
) {
    if (!features || !out_pcm || !out_sample_count || frame_count == 0) {
        return -1;
    }

    FARGANState fargan;
    float zeros[320] = {0};
    float in_features[5 * NB_FEATURES];
    uint32_t written = 0;

    fargan_init(&fargan);

#ifdef FARGAN_USE_INT8
    fargan_force_int8(&fargan);
#endif

#ifdef PITCH_CORRECTION_ENABLED
    float *feat_corrected = (float *)malloc(frame_count * NB_FEATURES * sizeof(float));
    if (!feat_corrected) return -1;
    memcpy(feat_corrected, features, frame_count * NB_FEATURES * sizeof(float));
    correct_pitch(feat_corrected, frame_count);
    const float *feat_ptr = feat_corrected;
#else
    const float *feat_ptr = features;
#endif

    for (uint32_t i = 0; i < 5; i++) {
        memcpy(&in_features[i * NB_FEATURES],
               &feat_ptr[0],
               NB_FEATURES * sizeof(float));
    }

    fargan_cont(&fargan, zeros, in_features);

    int skip = LPCNET_FRAME_SIZE / 2;
    int stop = 0;
    uint32_t frame_index = 0;

    while (1) {
        float fpcm[LPCNET_FRAME_SIZE];
        int16_t pcm[LPCNET_FRAME_SIZE];

        const float *current_features;

        if (stop || frame_index >= frame_count) {
            stop++;
            current_features = &feat_ptr[(frame_count - 1u) * NB_FEATURES];
        } else {
            current_features = &feat_ptr[frame_index * NB_FEATURES];
            frame_index++;
        }

        fargan_synthesize(&fargan, fpcm, current_features);

        for (uint32_t i = 0; i < LPCNET_FRAME_SIZE; i++) {
            pcm[i] = float_to_pcm16(fpcm[i]);
        }

        uint32_t start = (uint32_t)skip;
        uint32_t count = LPCNET_FRAME_SIZE - start;

        if (stop == 2) {
            count = LPCNET_FRAME_SIZE / 2;
        }

        if (written + count > max_samples) {
#ifdef PITCH_CORRECTION_ENABLED
            free(feat_corrected);
#endif
            return -2;
        }

        memcpy(out_pcm + written, pcm + start, count * sizeof(int16_t));
        written += count;

        if (stop == 2) {
            break;
        }

        skip = 0;
    }

#ifdef PITCH_CORRECTION_ENABLED
    free(feat_corrected);
#endif

    *out_sample_count = written;
    return 0;
}
