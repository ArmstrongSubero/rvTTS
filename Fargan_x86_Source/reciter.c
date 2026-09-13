#include <string.h>
#include "phonemes.h"
#include "reciter.h"

/* Fallback reciter, keeps an unknown word from becoming silence. */

static int starts_with(const char *s, const char *pat)
{
    while (*pat) {
        if (*s++ != *pat++) {
            return 0;
        }
    }
    return 1;
}

static void push(uint8_t ph, uint8_t *out, size_t *n, size_t max_out)
{
    if (*n < max_out) {
        out[*n] = ph;
        (*n)++;
    }
}

static void push2(uint8_t a, uint8_t b, uint8_t *out, size_t *n, size_t max_out)
{
    push(a, out, n, max_out);
    push(b, out, n, max_out);
}

static void push3(uint8_t a, uint8_t b, uint8_t c, uint8_t *out, size_t *n, size_t max_out)
{
    push(a, out, n, max_out);
    push(b, out, n, max_out);
    push(c, out, n, max_out);
}

size_t reciter_guess(const char *word, uint8_t *out, size_t max_out)
{
    size_t n = 0;
    size_t i = 0;
    size_t len = strlen(word);

    while (i < len) {
        const char *s = word + i;

        if (starts_with(s, "TION")) { push3(PH_SH, PH_AH, PH_N, out, &n, max_out); i += 4; continue; }
        if (starts_with(s, "SION")) { push3(PH_ZH, PH_AH, PH_N, out, &n, max_out); i += 4; continue; }
        if (starts_with(s, "ING"))  { push2(PH_IH, PH_NG, out, &n, max_out); i += 3; continue; }

        if (starts_with(s, "TH")) { push(PH_TH, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "SH")) { push(PH_SH, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "CH")) { push(PH_CH, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "NG")) { push(PH_NG, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "PH")) { push(PH_F, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "QU")) { push2(PH_K, PH_W, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "CK")) { push(PH_K, out, &n, max_out); i += 2; continue; }

        if (starts_with(s, "EE") || starts_with(s, "EA")) { push(PH_IY, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "OO")) { push(PH_UW, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "OU")) { push(PH_AW, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "OW")) { push(PH_OW, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "AI") || starts_with(s, "AY")) { push(PH_EY, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "OI") || starts_with(s, "OY")) { push(PH_OY, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "ER") || starts_with(s, "IR") || starts_with(s, "UR")) { push(PH_ER, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "AR")) { push2(PH_AA, PH_R, out, &n, max_out); i += 2; continue; }
        if (starts_with(s, "OR")) { push2(PH_AO, PH_R, out, &n, max_out); i += 2; continue; }

        char c = word[i];

        if (c == 'E' && i == len - 1 && len > 2) {
            i++;
            continue;
        }

        if (c == 'C') {
            char next = (i + 1 < len) ? word[i + 1] : 0;
            push((next == 'E' || next == 'I' || next == 'Y') ? PH_S : PH_K, out, &n, max_out);
            i++;
            continue;
        }

        if (c == 'G') {
            char next = (i + 1 < len) ? word[i + 1] : 0;
            push((next == 'E' || next == 'I' || next == 'Y') ? PH_JH : PH_G, out, &n, max_out);
            i++;
            continue;
        }

        switch (c) {
            case 'A': push(PH_AE, out, &n, max_out); break;
            case 'B': push(PH_B, out, &n, max_out); break;
            case 'D': push(PH_D, out, &n, max_out); break;
            case 'E': push(PH_EH, out, &n, max_out); break;
            case 'F': push(PH_F, out, &n, max_out); break;
            case 'H': push(PH_HH, out, &n, max_out); break;
            case 'I': push(PH_IH, out, &n, max_out); break;
            case 'J': push(PH_JH, out, &n, max_out); break;
            case 'K': push(PH_K, out, &n, max_out); break;
            case 'L': push(PH_L, out, &n, max_out); break;
            case 'M': push(PH_M, out, &n, max_out); break;
            case 'N': push(PH_N, out, &n, max_out); break;
            case 'O': push(PH_AA, out, &n, max_out); break;
            case 'P': push(PH_P, out, &n, max_out); break;
            case 'Q': push(PH_K, out, &n, max_out); break;
            case 'R': push(PH_R, out, &n, max_out); break;
            case 'S': push(PH_S, out, &n, max_out); break;
            case 'T': push(PH_T, out, &n, max_out); break;
            case 'U': push(PH_AH, out, &n, max_out); break;
            case 'V': push(PH_V, out, &n, max_out); break;
            case 'W': push(PH_W, out, &n, max_out); break;
            case 'X': push2(PH_K, PH_S, out, &n, max_out); break;
            case 'Y': push(PH_Y, out, &n, max_out); break;
            case 'Z': push(PH_Z, out, &n, max_out); break;
            default: break;
        }

        i++;
    }

    if (n == 0) {
        push(PH_SIL, out, &n, max_out);
    }

    return n;
}
