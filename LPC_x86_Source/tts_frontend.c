#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "tts_frontend.h"
#include "textnorm.h"
#include "pron_dict.h"
#include "reciter.h"
#include "duration_model.h"
#include "phonemes.h"

/*
 * Timing policy. Pauses come from punctuation, not from every word.
 * Function words compress, phrase final words lengthen, stress
 * stretches the vowel.
 */
#define PAUSE_COMMA_FRAMES       8u
#define PAUSE_SEMI_FRAMES        14u
#define PAUSE_SENTENCE_FRAMES    22u
#define PAUSE_PARAGRAPH_FRAMES   36u
#define PAUSE_AUTO_PHRASE_FRAMES 6u

#define PAUSE_LEADING_FRAMES     10u
#define PAUSE_TRAILING_FRAMES    12u

#define FUNC_WORD_MULT           0.82f
#define FUNC_WORD_FINAL_MULT     0.92f
#define UNSTRESSED_VOWEL_MULT    0.90f
#define PRIMARY_VOWEL_MULT       1.15f
#define SECONDARY_VOWEL_MULT     1.06f
#define PHRASE_FINAL_MULT        1.10f
#define SENTENCE_FINAL_MULT      1.14f
#define FINAL_TAIL_MULT          1.14f
#define AUTO_PHRASE_CONTENT_WORDS 7u

/* User lexicon: words CMUdict does not carry. Checked before the reciter. */
typedef struct {
    const char *word;
    const uint8_t *phones;
    const uint8_t *stress;
    uint8_t count;
} UserLexEntry;

static const uint8_t ulx_ph_ROVARI[] = { PH_R, PH_OW, PH_V, PH_AA, PH_R, PH_IY };
static const uint8_t ulx_st_ROVARI[] = { 0, 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_GUVARI[] = { PH_G, PH_UW, PH_V, PH_AA, PH_R, PH_IY };
static const uint8_t ulx_st_GUVARI[] = { 0, 0, 0, 1, 0, 0 };
static const uint8_t ulx_ph_BAOCHIP[] = { PH_B, PH_AW, PH_CH, PH_IH, PH_P };
static const uint8_t ulx_st_BAOCHIP[] = { 0, 1, 0, 0, 0 };
static const uint8_t ulx_ph_DABAO[] = { PH_D, PH_AA, PH_B, PH_AW };
static const uint8_t ulx_st_DABAO[] = { 0, 1, 0, 0 };
static const uint8_t ulx_ph_RISCV[] = { PH_R, PH_IH, PH_S, PH_K, PH_F, PH_AY, PH_V };
static const uint8_t ulx_st_RISCV[] = { 0, 1, 0, 0, 0, 1, 0 };

static const UserLexEntry user_lexicon[] = {
    { "BAOCHIP", ulx_ph_BAOCHIP, ulx_st_BAOCHIP, 5 },
    { "DABAO",   ulx_ph_DABAO,   ulx_st_DABAO,   4 },
    { "GUVARI",  ulx_ph_GUVARI,  ulx_st_GUVARI,  6 },
    { "RISCV",   ulx_ph_RISCV,   ulx_st_RISCV,   7 },
    { "ROVARI",  ulx_ph_ROVARI,  ulx_st_ROVARI,  6 }
};

static const UserLexEntry *user_lex_lookup(const char *word)
{
    size_t i;
    for (i = 0; i < sizeof(user_lexicon) / sizeof(user_lexicon[0]); i++) {
        if (strcmp(word, user_lexicon[i].word) == 0) {
            return &user_lexicon[i];
        }
    }
    return 0;
}

/* LJSpeech is an American reader, so the model learned American allophones. */

#ifndef TTS_FLAPPING
#define TTS_FLAPPING 0
#endif
#ifndef TTS_SHORT_INTERVOCALIC_STOPS
#define TTS_SHORT_INTERVOCALIC_STOPS 0
#endif

/* frames for a flap, roughly 30 ms */
#ifndef TTS_FLAP_FRAMES
#define TTS_FLAP_FRAMES 4u
#endif
/* how much of the table duration an intervocalic stop keeps */
#ifndef TTS_INTERVOCALIC_STOP_MULT
#define TTS_INTERVOCALIC_STOP_MULT 0.40f
#endif

static int is_vowel_phone(uint8_t ph);
static int is_stop_phone(uint8_t ph);

/* A flap needs a vowel or r before it and an unstressed vowel after. */
static int is_flap_left(uint8_t ph)
{
    return is_vowel_phone(ph) || ph == PH_R;
}

/* Tap /t/ and /d/ where an American speaker would. Runs per word. */
static void apply_flapping(uint8_t *phones, const uint8_t *stress, size_t n)
{
#if TTS_FLAPPING
    size_t j;
    for (j = 1; j + 1 < n; j++) {
        if (phones[j] != PH_T && phones[j] != PH_D) {
            continue;
        }
        if (!is_flap_left(phones[j - 1])) {
            continue;
        }
        if (!is_vowel_phone(phones[j + 1]) || stress[j + 1] != 0) {
            continue;
        }
        phones[j] = PH_D;
    }
#else
    (void)phones; (void)stress; (void)n;
#endif
}

/* index of the word currently being emitted, 0xFFFF for silence */
static uint16_t g_cur_word = 0xFFFFu;

static int append_phone(TtsFrontendResult *out, uint8_t phone, uint8_t stress)
{
    if (out->phone_count >= TTS_MAX_PHONES) {
        return -1;
    }

    out->phones[out->phone_count] = phone;
    out->stress[out->phone_count] = stress;
    out->phone_count++;
    return 0;
}

static int append_frame(TtsFrontendResult *out, uint8_t phone, uint8_t stress)
{
    if (out->frame_count >= TTS_MAX_FRAMES) {
        return -1;
    }

    out->frame_phones[out->frame_count] = phone;
    out->frame_stress[out->frame_count] = stress;
    out->frame_word[out->frame_count] = g_cur_word;
    out->frame_count++;
    return 0;
}

static int append_pause_frames(TtsFrontendResult *out, uint16_t frames)
{
    g_cur_word = 0xFFFFu;

    if (append_phone(out, PH_SIL, 0) != 0) {
        return -1;
    }

    for (uint16_t i = 0; i < frames; i++) {
        if (append_frame(out, PH_SIL, 0) != 0) {
            return -1;
        }
    }

    return 0;
}

static int is_pause_token(const char *w)
{
    return strcmp(w, "<COMMA>") == 0 ||
           strcmp(w, "<SEMI>") == 0 ||
           strcmp(w, "<SENT>") == 0 ||
           strcmp(w, "<PARA>") == 0 ||
           strcmp(w, "<SIL>") == 0;
}

static uint16_t pause_frames_for_token(const char *w)
{
    if (strcmp(w, "<COMMA>") == 0) return PAUSE_COMMA_FRAMES;
    if (strcmp(w, "<SEMI>") == 0)  return PAUSE_SEMI_FRAMES;
    if (strcmp(w, "<PARA>") == 0)  return PAUSE_PARAGRAPH_FRAMES;
    if (strcmp(w, "<AUTO>") == 0)  return PAUSE_AUTO_PHRASE_FRAMES;
    return PAUSE_SENTENCE_FRAMES;
}

static int is_sentence_pause(const char *w)
{
    return strcmp(w, "<SENT>") == 0 || strcmp(w, "<PARA>") == 0 || strcmp(w, "<SIL>") == 0;
}

static int is_vowel_phone(uint8_t ph)
{
    switch (ph) {
        case PH_AA: case PH_AE: case PH_AH: case PH_AO:
        case PH_AW: case PH_AY: case PH_EH: case PH_ER:
        case PH_EY: case PH_IH: case PH_IY: case PH_OW:
        case PH_OY: case PH_UH: case PH_UW:
            return 1;
        default:
            return 0;
    }
}

static int is_sonorant_phone(uint8_t ph)
{
    switch (ph) {
        case PH_L: case PH_M: case PH_N: case PH_NG:
        case PH_R: case PH_W: case PH_Y:
            return 1;
        default:
            return 0;
    }
}

static int is_function_word(const char *w)
{
    static const char *words[] = {
        "A", "AN", "AND", "ARE", "AS", "AT", "BE", "BY", "DO", "FOR",
        "FROM", "HAD", "HAS", "HAVE", "HE", "HER", "HIS", "I", "IF", "IN",
        "IS", "IT", "ITS", "ME", "MY", "OF", "ON", "OR", "OUR", "SHE",
        "THAT", "THE", "THEIR", "THEM", "THEN", "THERE", "THESE", "THIS",
        "TO", "WAS", "WE", "WERE", "WILL", "WITH", "YOU", "YOUR"
    };

    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (strcmp(w, words[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_content_word(const char *w)
{
    return !is_pause_token(w) && !is_function_word(w);
}

static int is_stop_phone(uint8_t ph)
{
    switch (ph) {
        case PH_P: case PH_T: case PH_K:
        case PH_B: case PH_D: case PH_G:
            return 1;
        default:
            return 0;
    }
}

static int is_voiceless_obstruent(uint8_t ph)
{
    switch (ph) {
        case PH_P: case PH_T: case PH_K:
        case PH_F: case PH_TH: case PH_S: case PH_SH: case PH_CH:
            return 1;
        default:
            return 0;
    }
}

static int is_voiced_obstruent(uint8_t ph)
{
    switch (ph) {
        case PH_B: case PH_D: case PH_G:
        case PH_DH: case PH_V: case PH_Z: case PH_ZH: case PH_JH:
            return 1;
        default:
            return 0;
    }
}

static uint16_t scale_duration(uint16_t base, float scale)
{
    int frames = (int)(base * scale + 0.5f);
    if (frames < 2) frames = 2;
    if (frames > 40) frames = 40;
    return (uint16_t)frames;
}

static int append_phone_to_frames_scaled(TtsFrontendResult *out,
                                         uint8_t phone,
                                         uint8_t stress,
                                         uint8_t prev_phone,
                                         uint8_t next_phone,
                                         float word_scale,
                                         int function_word,
                                         int final_region,
                                         int word_final)
{
    uint16_t base = duration_get_frames(phone);
    float scale = word_scale;

#if TTS_FLAPPING
    /* a tap has a fixed short duration regardless of the table */
    if (phone == PH_D && is_vowel_phone(prev_phone)
        && is_vowel_phone(next_phone)) {
        base = TTS_FLAP_FRAMES;
    }
#endif

    /* Vowel length follows stress and the class of the sound after it. */
    if (is_vowel_phone(phone)) {
        if (stress == 1) {
            scale *= PRIMARY_VOWEL_MULT;
        } else if (stress == 2) {
            scale *= SECONDARY_VOWEL_MULT;
        } else {
            scale *= UNSTRESSED_VOWEL_MULT;
        }

        if (function_word && stress == 0) {
            scale *= 0.86f;
        }

        if (next_phone != PH_SIL) {
            if (is_voiceless_obstruent(next_phone)) {
                scale *= 0.90f;
            } else if (is_voiced_obstruent(next_phone)) {
                scale *= 1.04f;
            } else if (is_sonorant_phone(next_phone)) {
                scale *= 1.07f;
            }
        }
    }

#if TTS_SHORT_INTERVOCALIC_STOPS
    /* Between two vowels a real stop is far shorter than the table average. */
    if (is_stop_phone(phone)
        && is_vowel_phone(prev_phone) && is_vowel_phone(next_phone)) {
        scale *= TTS_INTERVOCALIC_STOP_MULT;
    }
#endif

    /* Stops the cluster from stepping through machine like. */
    if (!is_vowel_phone(phone) && prev_phone != PH_SIL && next_phone != PH_SIL) {
        if (is_stop_phone(phone) && is_stop_phone(next_phone)) {
            scale *= 0.82f;
        } else if (is_voiceless_obstruent(phone) && is_voiceless_obstruent(next_phone)) {
            scale *= 0.90f;
        }
    }

    /* Do not over-hold final stops; lengthen mainly vowels and sonorants. */
    if (word_final && is_stop_phone(phone) && !final_region) {
        scale *= 0.92f;
    }

    if (final_region && (is_vowel_phone(phone) || is_sonorant_phone(phone))) {
        scale *= FINAL_TAIL_MULT;
    }

    uint16_t dur = scale_duration(base, scale);

    for (uint16_t i = 0; i < dur; i++) {
        if (append_frame(out, phone, stress) != 0) {
            return -1;
        }
    }

    return 0;
}

static int next_non_pause_index(const WordList *words, size_t start)
{
    for (size_t i = start; i < words->count; i++) {
        if (!is_pause_token(words->words[i])) {
            return (int)i;
        }
    }
    return -1;
}

static const char *next_pause_token(const WordList *words, size_t word_index)
{
    if (word_index + 1 < words->count && is_pause_token(words->words[word_index + 1])) {
        return words->words[word_index + 1];
    }
    return NULL;
}

static int append_word_pronunciation(TtsFrontendResult *out,
                                     const char *w,
                                     const uint8_t *phones,
                                     const uint8_t *stress,
                                     size_t phone_count,
                                     const char *pause_after)
{
    int function_word = is_function_word(w);
    int phrase_final = pause_after != NULL;

    g_cur_word = out->word_count;
    if (out->word_count < 0xFFFFu) out->word_count++;
    int sentence_final = pause_after && is_sentence_pause(pause_after);

    float word_scale = 1.0f;

    if (function_word && !phrase_final) {
        word_scale *= FUNC_WORD_MULT;
    } else if (function_word && phrase_final) {
        word_scale *= FUNC_WORD_FINAL_MULT;
    }

    if (phrase_final) {
        word_scale *= PHRASE_FINAL_MULT;
    }

    if (sentence_final) {
        word_scale *= SENTENCE_FINAL_MULT;
    }

    uint8_t local[64];
    if (phone_count <= sizeof(local)) {
        memcpy(local, phones, phone_count);
        apply_flapping(local, stress, phone_count);
        phones = local;
    }

    for (size_t j = 0; j < phone_count; j++) {
        if (append_phone(out, phones[j], stress[j]) != 0) {
            return -1;
        }

        /* Last two phones of a phrase-final word get tail lengthening. */
        int final_region = phrase_final && (j + 2 >= phone_count);
        int word_final = (j + 1 == phone_count);
        uint8_t prev_phone = (j > 0) ? phones[j - 1] : PH_SIL;
        uint8_t next_phone = (j + 1 < phone_count) ? phones[j + 1] : PH_SIL;

        if (append_phone_to_frames_scaled(out, phones[j], stress[j],
                                          prev_phone, next_phone,
                                          word_scale, function_word,
                                          final_region, word_final) != 0) {
            return -1;
        }
    }

    return 0;
}

int tts_frontend_process(const char *text, TtsFrontendResult *out, int verbose)
{
    WordList words;

    if (!text || !out) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    g_cur_word = 0xFFFFu;

    normalize_and_tokenize(text, &words);

    /* Leading silence: gives the GRU time to warm up */
    if (append_pause_frames(out, PAUSE_LEADING_FRAMES) != 0) {
        return -1;
    }

    unsigned content_words_since_pause = 0;

    for (size_t i = 0; i < words.count; i++) {
        const char *w = words.words[i];

        if (is_pause_token(w)) {
            if (append_pause_frames(out, pause_frames_for_token(w)) != 0) {
                return -1;
            }
            content_words_since_pause = 0;
            continue;
        }

        if (is_content_word(w)) {
            content_words_since_pause++;
        }

        const char *pause_after = next_pause_token(&words, i);
        int auto_phrase_break = 0;
        if (pause_after == NULL &&
            content_words_since_pause >= AUTO_PHRASE_CONTENT_WORDS &&
            is_content_word(w)) {
            int next_i = next_non_pause_index(&words, i + 1);
            if (next_i >= 0 && is_content_word(words.words[next_i])) {
                pause_after = "<AUTO>";
                auto_phrase_break = 1;
            }
        }
        const PronEntry *entry = pron_lookup(w);

        if (entry) {
            if (verbose) {
                printf("  %-16s dictionary  ", w);
                for (uint8_t j = 0; j < entry->phone_count; j++) {
                    printf("%s", phoneme_name(entry->phones[j]));
                    if (entry->stress[j]) printf("%u", (unsigned)entry->stress[j]);
                    if (j + 1 < entry->phone_count) printf(" ");
                }
                printf("\n");
            }

            if (append_word_pronunciation(out, w,
                                          entry->phones,
                                          entry->stress,
                                          entry->phone_count,
                                          pause_after) != 0) {
                return -1;
            }
        } else if (user_lex_lookup(w) != 0) {
            const UserLexEntry *u = user_lex_lookup(w);

            if (append_word_pronunciation(out, w, u->phones, u->stress,
                                          u->count, pause_after) != 0) {
                return -1;
            }
        } else {
            uint8_t tmp[64];
            uint8_t tmp_stress[64];
            size_t n = reciter_guess(w, tmp, sizeof(tmp));
            memset(tmp_stress, 0, sizeof(tmp_stress));

            if (verbose) {
                printf("  %-16s reciter     ", w);
                for (size_t j = 0; j < n; j++) {
                    printf("%s", phoneme_name(tmp[j]));
                    if (j + 1 < n) printf(" ");
                }
                printf("\n");
            }

            if (append_word_pronunciation(out, w, tmp, tmp_stress, n, pause_after) != 0) {
                return -1;
            }
        }

        if (auto_phrase_break) {
            if (append_pause_frames(out, PAUSE_AUTO_PHRASE_FRAMES) != 0) {
                return -1;
            }
            content_words_since_pause = 0;
        }
    }

    /* Trailing silence: prevents FARGAN from cutting off the last phone */
    if (append_pause_frames(out, PAUSE_TRAILING_FRAMES) != 0) {
        return -1;
    }

    return 0;
}

static void print_words(const char *text)
{
    WordList words;
    normalize_and_tokenize(text, &words);

    printf("\nWords:\n");
    for (size_t i = 0; i < words.count; i++) {
        printf("  %s\n", words.words[i]);
    }
}

static void print_pronunciations(const char *text)
{
    WordList words;
    normalize_and_tokenize(text, &words);

    printf("\nPronunciations:\n");

    for (size_t i = 0; i < words.count; i++) {
        const char *w = words.words[i];

        if (is_pause_token(w)) {
            printf("  %-16s pause       <sil> x %u frames\n", w, (unsigned)pause_frames_for_token(w));
            continue;
        }

        const PronEntry *entry = pron_lookup(w);

        if (entry) {
            printf("  %-16s dictionary  ", w);
            for (uint8_t j = 0; j < entry->phone_count; j++) {
                printf("%s", phoneme_name(entry->phones[j]));
                if (entry->stress[j]) printf("%u", (unsigned)entry->stress[j]);
                if (j + 1 < entry->phone_count) printf(" ");
            }
            printf("\n");
        } else {
            uint8_t tmp[64];
            size_t n = reciter_guess(w, tmp, sizeof(tmp));

            printf("  %-16s reciter     ", w);
            for (size_t j = 0; j < n; j++) {
                printf("%s", phoneme_name(tmp[j]));
                if (j + 1 < n) printf(" ");
            }
            printf("\n");
        }
    }
}

static unsigned count_frames_for_phone(const TtsFrontendResult *result, size_t phone_index)
{
    uint8_t phone = result->phones[phone_index];
    uint8_t stress = result->stress[phone_index];
    /* Approximate: the compact result does not keep a phone to frame map. */
    (void)result;
    (void)stress;
    if (phone == PH_SIL) return 0;
    return (unsigned)duration_get_frames(phone);
}

void tts_debug_print(const char *text)
{
    TtsFrontendResult result;

    if (tts_frontend_process(text, &result, 0) != 0) {
        printf("Frontend failed.\n");
        return;
    }

    printf("Text:\n  %s\n", text);

    print_words(text);
    print_pronunciations(text);

    printf("\nPhoneme sequence:\n  ");
    for (size_t i = 0; i < result.phone_count; i++) {
        printf("%s", phoneme_name(result.phones[i]));
        if (result.stress[i]) printf("%u", (unsigned)result.stress[i]);
        if (i + 1 < result.phone_count) printf(" ");
    }
    printf("\n");

    printf("\nBase durations, in 10 ms frames; actual frame stream includes prosody scaling:\n");
    for (size_t i = 0; i < result.phone_count; i++) {
        char label[16];
#if defined(_MSC_VER)
        _snprintf(label, sizeof(label), "%s", phoneme_name(result.phones[i]));
        label[sizeof(label) - 1] = '\0';
#else
        snprintf(label, sizeof(label), "%s", phoneme_name(result.phones[i]));
#endif
        if (result.stress[i] && strlen(label) < sizeof(label) - 2) {
            size_t n = strlen(label);
            label[n] = (char)('0' + result.stress[i]);
            label[n + 1] = '\0';
        }

        if (result.phones[i] == PH_SIL) {
            printf("  %-5s pause-token\n", label);
        } else {
            printf("  %-5s %u\n", label, count_frames_for_phone(&result, i));
        }
    }

    printf("\nFrame-level labels:\n  ");
    for (size_t i = 0; i < result.frame_count; i++) {
        printf("%s", phoneme_name(result.frame_phones[i]));
        if (result.frame_stress[i]) printf("%u", (unsigned)result.frame_stress[i]);
        if (i + 1 < result.frame_count) printf(" ");
    }
    printf("\n");

    printf("\nTotal phones: %u\n", (unsigned)result.phone_count);
    printf("Total frames: %u\n", (unsigned)result.frame_count);
    printf("Approx duration: %.2f seconds\n", (double)result.frame_count * 0.01);
}
