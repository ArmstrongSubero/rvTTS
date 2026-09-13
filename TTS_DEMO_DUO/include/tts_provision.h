/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2025 Rovari, rvembedded.com
 *
 * tts_provision.h
 *
 * Flash map and boot time provisioning for the weight blobs. The V5F
 * image address 0x00040000 is fixed by BAREW, Link_v5f.ld and
 * targets.py, so the blobs are placed around it.
 *
 *   0x00000000  V3F code            56 KB
 *   0x0000E000  prosody weights    196 KB
 *   0x00040000  V5F code            64 KB
 *   0x00050000  word vocab         109 KB
 *   0x0006C000  free                48 KB
 *   0x00078000  acoustic weights   397 KB   bank 1
 *   0x000DC000  manifest             8 KB
 *   0x000DE000  free                72 KB
 *
 * Addresses are in the direct view. The vendor flash calls take the
 * alias view at 0x08000000, which TTS_ALIAS() supplies.
 */

#ifndef TTS_PROVISION_H
#define TTS_PROVISION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TTS_V3F_CODE_END    0x0000E000u

#define TTS_PROSODY_ADDR    0x0000E000u
#define TTS_PROSODY_LEN     201120u

#define TTS_V5F_CODE_ADDR   0x00040000u
#define TTS_V5F_CODE_END    0x00050000u

#define TTS_VOCAB_ADDR      0x00050000u
#define TTS_VOCAB_LEN       111947u

#define TTS_ACOUSTIC_ADDR   0x00078000u
#define TTS_ACOUSTIC_LEN    406524u

#define TTS_MANIFEST_ADDR   0x000DC000u

#define TTS_BANK1_BASE      0x00078000u
#define TTS_FLASH_END       0x000F0000u

#define TTS_ALIAS(a)        ((a) | 0x08000000u)

#define TTS_ACOUSTIC_PATH   "0:/ACOUSTIC.BIN"
#define TTS_PROSODY_PATH    "0:/PROSODY.BIN"
#define TTS_VOCAB_PATH      "0:/WORDVOC.BIN"

#define TTS_BLOB_COUNT      3u

typedef enum {
    TTS_PROV_OK           = 0,
    TTS_PROV_ALREADY      = 1,
    TTS_PROV_ERR_SD       = 2,
    TTS_PROV_ERR_OPEN     = 3,
    TTS_PROV_ERR_SIZE     = 4,
    TTS_PROV_ERR_READ     = 5,
    TTS_PROV_ERR_ERASE    = 6,
    TTS_PROV_ERR_WRITE    = 7,
    TTS_PROV_ERR_VERIFY   = 8,
    TTS_PROV_ERR_MANIFEST = 9
} tts_prov_status_t;

/* V3F only. Call once from app_init before commanding the V5F. */
tts_prov_status_t tts_provision_ensure(void);
tts_prov_status_t tts_provision_invalidate(void);
const char *tts_provision_strerror(tts_prov_status_t s);

#ifdef __cplusplus
}
#endif

#endif /* TTS_PROVISION_H */
