#ifndef PHONEMES_H
#define PHONEMES_H

#include <stdint.h>

/* ARPAbet style phoneme IDs, the contract from frontend to features. */
typedef enum {
    PH_SIL = 0,
    PH_BRK,

    PH_AA,
    PH_AE,
    PH_AH,
    PH_AO,
    PH_AW,
    PH_AY,
    PH_B,
    PH_CH,
    PH_D,
    PH_DH,
    PH_EH,
    PH_ER,
    PH_EY,
    PH_F,
    PH_G,
    PH_HH,
    PH_IH,
    PH_IY,
    PH_JH,
    PH_K,
    PH_L,
    PH_M,
    PH_N,
    PH_NG,
    PH_OW,
    PH_OY,
    PH_P,
    PH_R,
    PH_S,
    PH_SH,
    PH_T,
    PH_TH,
    PH_UH,
    PH_UW,
    PH_V,
    PH_W,
    PH_Y,
    PH_Z,
    PH_ZH,

    PH_COUNT
} PhonemeId;

static inline const char *phoneme_name(uint8_t ph)
{
    switch (ph) {
        case PH_SIL: return "<sil>";
        case PH_BRK: return "<brk>";
        case PH_AA: return "AA";
        case PH_AE: return "AE";
        case PH_AH: return "AH";
        case PH_AO: return "AO";
        case PH_AW: return "AW";
        case PH_AY: return "AY";
        case PH_B: return "B";
        case PH_CH: return "CH";
        case PH_D: return "D";
        case PH_DH: return "DH";
        case PH_EH: return "EH";
        case PH_ER: return "ER";
        case PH_EY: return "EY";
        case PH_F: return "F";
        case PH_G: return "G";
        case PH_HH: return "HH";
        case PH_IH: return "IH";
        case PH_IY: return "IY";
        case PH_JH: return "JH";
        case PH_K: return "K";
        case PH_L: return "L";
        case PH_M: return "M";
        case PH_N: return "N";
        case PH_NG: return "NG";
        case PH_OW: return "OW";
        case PH_OY: return "OY";
        case PH_P: return "P";
        case PH_R: return "R";
        case PH_S: return "S";
        case PH_SH: return "SH";
        case PH_T: return "T";
        case PH_TH: return "TH";
        case PH_UH: return "UH";
        case PH_UW: return "UW";
        case PH_V: return "V";
        case PH_W: return "W";
        case PH_Y: return "Y";
        case PH_Z: return "Z";
        case PH_ZH: return "ZH";
        default: return "?";
    }
}

#endif /* PHONEMES_H */
