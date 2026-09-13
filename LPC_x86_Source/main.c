/*
 * main.c
 *
 * Text to speech with the LPC source filter vocoder. Plain C99, no
 * platform headers, no libm in the vocoder, no weight blob.
 *
 *   Neural_LPC "The temperature is thirty seven degrees" out.wav
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#include "tts_frontend.h"
#include "prosody_model.h"
#include "acoustic_stage2.h"
#include "word_vocab.h"
#include "pron_dict.h"
#include "lpcvoc.h"

#define DIM 20
#define NB_BANDS 18
#define SPEAKER_PITCH_MEAN 0.2120f
#define SPEAKER_PITCH_STD  0.3615f

static void put32(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f);
                                         fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static void write_wav(const char *path, const int16_t *pcm, unsigned n, unsigned sr)
{
    FILE *f = fopen(path, "wb");
    unsigned bytes = n * 2;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); put32(f, 16); put16(f, 1); put16(f, 1);
    put32(f, sr); put32(f, sr * 2); put16(f, 2); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    fwrite(pcm, 2, n, f);
    fclose(f);
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Over the whole utterance. Per chunk it flattens the sentence contour. */
static unsigned char *slurp(const char *path, long *len_out)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long len;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc(len);
    if (!buf || fread(buf, 1, len, f) != (size_t)len) { fclose(f); free(buf); return 0; }
    fclose(f);
    *len_out = len;
    return buf;
}

static void correct_pitch(float *feat, unsigned n)
{
    double sum = 0, sq = 0;
    unsigned i;
    for (i = 0; i < n; i++) { float p = feat[i * DIM + NB_BANDS]; sum += p; sq += (double)p * p; }
    {
        float mean = (float)(sum / n);
        double var = sq / n - (double)mean * mean;
        float sd = var > 1e-10 ? (float)sqrt(var) : 1e-5f;
        for (i = 0; i < n; i++) {
            float *p = &feat[i * DIM + NB_BANDS];
            float z = (*p - mean) / sd;
            if (z > 2.5f) z = 2.5f;
            if (z < -2.5f) z = -2.5f;
            *p = z * SPEAKER_PITCH_STD + SPEAKER_PITCH_MEAN;
        }
    }
}

int main(int argc, char **argv)
{
    const char *text = (argc > 1) ? argv[1] : "The meter is reading forty five point five";
    const char *out = (argc > 2) ? argv[2] : "out.wav";

    static TtsFrontendResult r;
    static LpcVocState voc;
    ProsodyModel pm;
    AcousticStage2 am;
    uint16_t *wid;
    float *energy, *pitch, *voicing, *feat;
    int16_t *pcm;
    unsigned n, t;
    double t0, t1;

    {
        long n;
        unsigned char *d = slurp("pron_dict.bin", &n);
        if (!d || pron_dict_init(d, (uint32_t)n) != 0) {
            printf("cannot load pron_dict.bin\n");
            return 1;
        }
        printf("dictionary: %u entries, %ld bytes, used in place\n",
               pron_dict_entries(), n);
    }

    if (word_vocab_load("word_vocab.json") != 0)
        printf("warning: word_vocab.json not found\n");

    if (tts_frontend_process(text, &r, 0) != 0) {
        printf("frontend failed (over %u words or %u frames?)\n",
               (unsigned)TTS_MAX_WORDS, (unsigned)TTS_MAX_FRAMES);
        return 1;
    }
    n = (unsigned)r.frame_count;

    if (prosody_load(&pm, "prosody_weights_int8.bin") != 0) return 1;
    if (acoustic_stage2_load(&am, "acoustic_stage2_weights_int8.bin") != 0) return 1;

    wid = (uint16_t *)calloc(n, sizeof(uint16_t));
    energy = (float *)malloc(n * sizeof(float));
    pitch = (float *)malloc(n * sizeof(float));
    voicing = (float *)malloc(n * sizeof(float));
    feat = (float *)calloc((size_t)n * DIM, sizeof(float));
    pcm = (int16_t *)malloc((size_t)n * LPCVOC_FRAME * sizeof(int16_t));

    word_vocab_assign_frames_indexed(text, r.frame_word, n, wid);
    prosody_generate(&pm, r.frame_phones, r.frame_stress, wid, n,
                     energy, pitch, voicing);
    acoustic_stage2_generate(&am, r.frame_phones, r.frame_stress,
                             energy, pitch, voicing, n, feat);
    for (t = 0; t < n; t++) {
        feat[t * DIM + 0] = energy[t];
        feat[t * DIM + 18] = pitch[t];
        feat[t * DIM + 19] = voicing[t];
    }
    correct_pitch(feat, n);

    lpcvoc_init(&voc);
    t0 = now_ms();
    for (t = 0; t < n; t++)
        lpcvoc_frame(&voc, feat + (size_t)t * DIM, pcm + (size_t)t * LPCVOC_FRAME);
    t1 = now_ms();

    write_wav(out, pcm, n * LPCVOC_FRAME, LPCVOC_SAMPLE_RATE);

    printf("text:         \"%s\"\n", text);
    printf("vocoder:      LPC order %d, source filter\n", LPCVOC_ORDER);
    printf("frames:       %u  (%.2f s of audio)\n", n, n * 0.01);
    printf("vocoder time  %.2f ms   RTF %.5fx\n", t1 - t0, (t1 - t0) / (n * 10.0));
    printf("state:        %u bytes   weights: none\n", (unsigned)sizeof(LpcVocState));
    printf("wrote %s\n", out);

    free(pcm); free(feat); free(voicing); free(pitch); free(energy);
    free(wid);
    prosody_free(&pm);
    acoustic_stage2_free(&am);
    word_vocab_free();
    return 0;
}
