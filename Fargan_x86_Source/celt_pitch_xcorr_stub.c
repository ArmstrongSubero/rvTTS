#include "arch.h"

/* Portable fallback for celt_pitch_xcorr_c, used by the celt_lpc.c autocorrelation path. */
void celt_pitch_xcorr_c(const opus_val16 *_x,
                        const opus_val16 *_y,
                        opus_val32 *xcorr,
                        int len,
                        int max_pitch,
                        int arch)
{
    int i;
    int j;

    (void)arch;

    for (i = 0; i < max_pitch; i++) {
        opus_val32 sum = 0;

        for (j = 0; j < len; j++) {
            sum += (opus_val32)_x[j] * (opus_val32)_y[i + j];
        }

        xcorr[i] = sum;
    }
}
