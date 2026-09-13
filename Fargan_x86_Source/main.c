/*
 * main.c
 *
 * Two stage TTS pipeline, v8. Synthesis runs in chunks and each stage
 * is timed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "tts_frontend.h"
#include "prosody_model.h"
#include "acoustic_stage2.h"
#include "word_vocab.h"
#include "fargan_wrapper.h"
#include "wav_writer.h"
#include "phonemes.h"
#include "frame_export.h"
#include "feature_export.h"

#define FARGAN_FEATURE_DIM TTS_FARGAN_FEATURE_DIM
#define MAX_CHUNK_FRAMES 100u

static double get_time_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

static int synthesize_features_to_wav(const char *fp, const char *wp)
{
    FILE *f = fopen(fp, "rb");
    if (!f) { printf("ERROR: Cannot open: %s\n", fp); return 1; }
    fseek(f, 0, SEEK_END); long bytes = ftell(f); fseek(f, 0, SEEK_SET);
    if (bytes <= 0 || (bytes % (FARGAN_FEATURE_DIM * sizeof(float))) != 0) {
        printf("ERROR: Bad size: %ld\n", bytes); fclose(f); return 1;
    }
    uint32_t frames = (uint32_t)(bytes / (FARGAN_FEATURE_DIM * sizeof(float)));
    float *feat = (float *)malloc((size_t)frames * FARGAN_FEATURE_DIM * sizeof(float));
    if (!feat) { fclose(f); return 1; }
    fread(feat, sizeof(float), (size_t)frames * FARGAN_FEATURE_DIM, f); fclose(f);
    uint32_t max_s = (frames + 4u) * 160u;
    int16_t *pcm = (int16_t *)malloc((size_t)max_s * sizeof(int16_t));
    if (!pcm) { free(feat); return 1; }
    uint32_t sc = 0;
    tts_fargan_synthesize_pcm16(feat, frames, pcm, max_s, &sc);
    wav_write_mono16(wp, pcm, sc, TTS_SAMPLE_RATE);
    free(pcm); free(feat); return 0;
}

static uint32_t find_chunks(const uint8_t *fp, uint32_t fc,
    uint32_t *starts, uint32_t *lengths, uint32_t max)
{
    uint32_t n = 0, cs = 0;
    while (cs < fc && n < max) {
        uint32_t rem = fc - cs, cl;
        if (rem <= MAX_CHUNK_FRAMES) { cl = rem; }
        else {
            cl = MAX_CHUNK_FRAMES;
            for (uint32_t i = MAX_CHUNK_FRAMES; i > MAX_CHUNK_FRAMES / 2; i--) {
                if (fp[cs + i] == PH_SIL) { cl = i + 1; break; }
            }
        }
        starts[n] = cs; lengths[n] = cl; n++; cs += cl;
    }
    return n;
}

int main(int argc, char **argv)
{
    if (argc >= 4 && strcmp(argv[1], "--features") == 0)
        return synthesize_features_to_wav(argv[2], argv[3]);

    const char *text = "The temperature is thirty degrees";
    const char *prefix = "tts_output";
    const char *pw_path = "prosody_weights_int8.bin";
    const char *aw_path = "acoustic_stage2_weights_int8.bin";
    const char *voc_path = "word_vocab.json";

    if (argc > 1) text = argv[1];
    if (argc > 2) prefix = argv[2];

    double t_wall_start = get_time_ms();

    double t0 = get_time_ms();
    if (word_vocab_load(voc_path) != 0)
        printf("Warning: word vocab not loaded.\n");
    double t_vocab = get_time_ms() - t0;

    printf("Text: \"%s\"\n\n", text);
    TtsFrontendResult result;
    t0 = get_time_ms();
    if (tts_frontend_process(text, &result, 0) != 0) {
        printf("Frontend failed.\n"); return 1;
    }
    double t_frontend = get_time_ms() - t0;

    uint32_t total_frames = (uint32_t)result.frame_count;
    double audio_duration = (double)total_frames * 0.01;
    printf("Frames: %u (%.2f seconds of audio)\n", (unsigned)total_frames, audio_duration);

    uint16_t *word_ids = (uint16_t *)calloc(total_frames, sizeof(uint16_t));
    if (!word_ids) { printf("OOM\n"); return 1; }
    word_vocab_assign_frames(text, result.frame_phones, total_frames, word_ids);

    ProsodyModel prosody;
    t0 = get_time_ms();
    if (prosody_load(&prosody, pw_path) != 0) { free(word_ids); return 1; }
    double t_load_prosody = get_time_ms() - t0;

    AcousticStage2 acoustic;
    t0 = get_time_ms();
    if (acoustic_stage2_load(&acoustic, aw_path) != 0) {
        prosody_free(&prosody); free(word_ids); return 1;
    }
    double t_load_acoustic = get_time_ms() - t0;

    #define MAX_CHUNKS 64
    uint32_t cstarts[MAX_CHUNKS], clens[MAX_CHUNKS];
    uint32_t n_chunks = find_chunks(result.frame_phones, total_frames,
                                     cstarts, clens, MAX_CHUNKS);

    uint32_t total_max_pcm = (total_frames + 4u * n_chunks) * 160u;
    int16_t *all_pcm = (int16_t *)malloc((size_t)total_max_pcm * sizeof(int16_t));
    if (!all_pcm) { printf("OOM\n"); return 1; }

    uint32_t total_samples = 0;
    double sum_prosody_ms = 0, sum_acoustic_ms = 0, sum_fargan_ms = 0;

    printf("\n--- Chunked synthesis: %u chunks (max %u frames) ---\n\n",
           (unsigned)n_chunks, (unsigned)MAX_CHUNK_FRAMES);
    printf("%-6s %6s %10s %10s %10s %10s\n",
           "Chunk", "Frames", "Prosody", "Acoustic", "FARGAN", "Total(ms)");
    printf("------ ------ ---------- ---------- ---------- ----------\n");

    for (uint32_t c = 0; c < n_chunks; c++) {
        uint32_t start = cstarts[c];
        uint32_t len = clens[c];
        double chunk_pros = 0, chunk_acou = 0, chunk_frgn = 0;

        /* Stage 1: Prosody */
        float *energy  = (float *)malloc(len * sizeof(float));
        float *pitch   = (float *)malloc(len * sizeof(float));
        float *voicing = (float *)malloc(len * sizeof(float));
        if (!energy || !pitch || !voicing) {
            printf("OOM chunk %u\n", c); break;
        }

        t0 = get_time_ms();
        prosody_generate(&prosody,
            result.frame_phones + start, result.frame_stress + start,
            word_ids + start, len, energy, pitch, voicing);
        chunk_pros = get_time_ms() - t0;

        /* Stage 2: Acoustic */
        float *features = (float *)calloc((size_t)len * FARGAN_FEATURE_DIM, sizeof(float));
        if (!features) { free(energy); free(pitch); free(voicing); break; }

        t0 = get_time_ms();
        acoustic_stage2_generate(&acoustic,
            result.frame_phones + start, result.frame_stress + start,
            energy, pitch, voicing, len, features);
        chunk_acou = get_time_ms() - t0;

            for (uint32_t t = 0; t < len; t++) {
            float *f = features + (size_t)t * FARGAN_FEATURE_DIM;
            f[0]  = energy[t];
            f[18] = pitch[t];
            f[19] = voicing[t];
        }
        free(energy); free(pitch); free(voicing);

        /* Stage 3: FARGAN */
        uint32_t chunk_pcm = 0;
        uint32_t rem_pcm = total_max_pcm - total_samples;

        t0 = get_time_ms();
        tts_fargan_synthesize_pcm16(features, len,
            all_pcm + total_samples, rem_pcm, &chunk_pcm);
        chunk_frgn = get_time_ms() - t0;

        free(features);
        total_samples += chunk_pcm;

        double chunk_total = chunk_pros + chunk_acou + chunk_frgn;
        sum_prosody_ms  += chunk_pros;
        sum_acoustic_ms += chunk_acou;
        sum_fargan_ms   += chunk_frgn;

        printf("  %2u   %5u   %8.1f   %8.1f   %8.1f   %8.1f\n",
               (unsigned)(c + 1), (unsigned)len,
               chunk_pros, chunk_acou, chunk_frgn, chunk_total);
    }

    double sum_compute = sum_prosody_ms + sum_acoustic_ms + sum_fargan_ms;
    printf("------ ------ ---------- ---------- ---------- ----------\n");
    printf(" Total %5u   %8.1f   %8.1f   %8.1f   %8.1f\n",
           (unsigned)total_frames,
           sum_prosody_ms, sum_acoustic_ms, sum_fargan_ms, sum_compute);

    char wav_path[256];
    snprintf(wav_path, sizeof(wav_path), "%s.wav", prefix);
    wav_write_mono16(wav_path, all_pcm, total_samples, TTS_SAMPLE_RATE);

#ifdef _WIN32
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "powershell -c \"(New-Object Media.SoundPlayer '%s').PlaySync()\"", wav_path);
        system(cmd);
    }
#endif

    double t_wall_total = get_time_ms() - t_wall_start;
    double audio_sec = (double)total_samples / TTS_SAMPLE_RATE;
    double rtf = (sum_compute / 1000.0) / audio_sec;

    double pros_per_sec = sum_prosody_ms / audio_sec;
    double acou_per_sec = sum_acoustic_ms / audio_sec;
    double frgn_per_sec = sum_fargan_ms / audio_sec;
    double total_per_sec = sum_compute / audio_sec;

    printf("\n");
    printf("============================================================\n");
    printf("  PERFORMANCE REPORT\n");
    printf("============================================================\n");
    printf("\n");
    printf("  Audio\n");
    printf("    Duration:            %.2f seconds\n", audio_sec);
    printf("    Frames:              %u\n", (unsigned)total_frames);
    printf("    Chunks:              %u (max %u frames)\n",
           (unsigned)n_chunks, (unsigned)MAX_CHUNK_FRAMES);
    printf("    Sample rate:         %u Hz\n", (unsigned)TTS_SAMPLE_RATE);
    printf("    Samples:             %u\n", (unsigned)total_samples);
    printf("\n");
    printf("  Measured timing (PC, x86, no SIMD)\n");
    printf("    Vocab load:          %.1f ms\n", t_vocab);
    printf("    Frontend:            %.1f ms\n", t_frontend);
    printf("    Prosody load:        %.1f ms\n", t_load_prosody);
    printf("    Acoustic load:       %.1f ms\n", t_load_acoustic);
    printf("    Prosody inference:   %.1f ms\n", sum_prosody_ms);
    printf("    Acoustic inference:  %.1f ms\n", sum_acoustic_ms);
#ifdef FARGAN_USE_INT8
    printf("    FARGAN inference:    %.1f ms (int8 GEMV)\n", sum_fargan_ms);
#else
    printf("    FARGAN inference:    %.1f ms (float32 GEMV)\n", sum_fargan_ms);
#endif
    printf("    Total inference:     %.1f ms\n", sum_compute);
    printf("    Wall clock:          %.1f ms\n", t_wall_total);
    printf("    Real-time factor:    %.3fx\n", rtf);
    printf("\n");
    printf("  Per second of audio (measured, x86)\n");
    printf("    Prosody:             %.1f ms/s (%.1f%%)\n",
           pros_per_sec, 100.0 * sum_prosody_ms / sum_compute);
    printf("    Acoustic:            %.1f ms/s (%.1f%%)\n",
           acou_per_sec, 100.0 * sum_acoustic_ms / sum_compute);
    printf("    FARGAN:              %.1f ms/s (%.1f%%)\n",
           frgn_per_sec, 100.0 * sum_fargan_ms / sum_compute);
    printf("    Total:               %.1f ms/s\n", total_per_sec);
    printf("\n");
    printf("  Model sizes (on flash)\n");
    printf("    Prosody:             %zu bytes (%.1f KB)\n",
           prosody.blob_size, (double)prosody.blob_size / 1024.0);
    printf("    Acoustic:            %zu bytes (%.1f KB)\n",
           acoustic.blob_size, (double)acoustic.blob_size / 1024.0);
#ifdef FARGAN_USE_INT8
    printf("    FARGAN:              ~820 KB compiled (int8 path active)\n");
#else
    printf("    FARGAN:              ~820 KB compiled (float32 path)\n");
#endif
    printf("    Total:               ~%.1f KB\n",
           (double)(prosody.blob_size + acoustic.blob_size) / 1024.0 + 820.0);
    printf("\n");
    printf("  Peak SRAM per chunk:   ~151 KB (acoustic stage)\n");
    printf("  WAV output:            %s\n", wav_path);
    printf("============================================================\n");

    free(all_pcm);
    free(word_ids);
    prosody_free(&prosody);
    acoustic_stage2_free(&acoustic);
    word_vocab_free();

    return 0;
}
