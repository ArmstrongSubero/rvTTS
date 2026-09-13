/*
 * lpcvoc_disp.h
 *
 * Pulse dispersion filter, generated, do not edit. Phase only, the
 * magnitude spectrum is untouched. Group delay ramps from 20 samples
 * at DC to 0 at Nyquist, ripple 2.02 dB from 100 Hz to 7.5 kHz.
 */

#ifndef LPCVOC_DISP_H
#define LPCVOC_DISP_H

#define LPCVOC_DISP_TAPS 49

static const float lpcvoc_disp[49] = {
    0.11200578f, -0.17113741f, 0.23888942f, -0.28985207f, 0.26532705f, -0.11105525f,
    -0.15223461f, 0.32067672f, -0.14886328f, -0.24032260f, 0.22065325f, 0.24032260f,
    -0.14886328f, -0.32067672f, -0.15223461f, 0.11105525f, 0.26532705f, 0.28985207f,
    0.23888942f, 0.17113741f, 0.11200578f, 0.07081861f, 0.04348266f, 0.02726406f,
    0.01694331f, 0.01117917f, 0.00723891f, 0.00514982f, 0.00347467f, 0.00267598f,
    0.00185723f, 0.00154177f, 0.00108432f, 0.00096449f, 0.00067871f, 0.00064340f,
    0.00044871f, 0.00044927f, 0.00029701f, 0.00029262f, 0.00017470f, 0.00016589f,
    0.00008714f, 0.00007665f, 0.00003322f, 0.00002418f, 0.00000689f, 0.00000228f,
    0.00000000f,
};

#endif /* LPCVOC_DISP_H */
