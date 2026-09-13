#include "duration_model.h"
#include "phonemes.h"

/* Phone durations in 10 ms frames, from LJSpeech MFA averages. */

uint16_t duration_get_frames(uint8_t ph)
{
    switch (ph) {
        case PH_SIL: return 12;
        case PH_BRK: return 3;
        case PH_AA: return 7;
        case PH_AE: return 10;
        case PH_AH: return 6;
        case PH_AO: return 8;
        case PH_AW: return 18;
        case PH_AY: return 17;
        case PH_B: return 5;
        case PH_CH: return 13;
        case PH_D: return 6;
        case PH_DH: return 4;
        case PH_EH: return 9;
        case PH_ER: return 11;
        case PH_EY: return 13;
        case PH_F: return 12;
        case PH_G: return 7;
        case PH_HH: return 7;
        case PH_IH: return 6;
        case PH_IY: return 8;
        case PH_JH: return 9;
        case PH_K: return 10;
        case PH_L: return 8;
        case PH_M: return 7;
        case PH_N: return 6;
        case PH_NG: return 7;
        case PH_OW: return 13;
        case PH_OY: return 17;
        case PH_P: return 10;
        case PH_R: return 6;
        case PH_S: return 12;
        case PH_SH: return 13;
        case PH_T: return 8;
        case PH_TH: return 10;
        case PH_UH: return 2;
        case PH_UW: return 10;
        case PH_V: return 6;
        case PH_W: return 10;
        case PH_Y: return 11;
        case PH_Z: return 9;
        case PH_ZH: return 9;
        default: return 5;
    }
}
