/*
 * tts_mailbox.h
 *
 * Inter core interface, included by app.rova and app_v5f.rova only.
 * The mailbox is at 0x2017F000, the top 4K of the shared block, which
 * Link_v3f.ld keeps out of the V3F RAM region.
 *
 * V3F owns the 443K of shared RAM below it: card, frontend, prosody
 * arena, PCM buffer, DAC, console.
 * V5F owns 128K ITCM and 256K DTCM: prosody, acoustic, vocoder, and
 * the weight blobs in its flash slot.
 *
 * Anything the V5F needs that will not fit DTCM is allocated by the
 * V3F and passed as a pointer. The whole utterance is rendered to PCM
 * before playback, so the V3F cannot underrun. SRAM is Normal memory
 * on the V5F, so every handoff is fenced on both sides.
 */

#ifndef TTS_MAILBOX_H
#define TTS_MAILBOX_H

#include <stdint.h>

#define MB              ((volatile uint32_t *)0x2017F000)

#define MB_CMD          MB[0]    /* V3F writes, V5F reads  */
#define MB_READY        MB[1]    /* V5F writes 0xCAFE once up */
#define MB_FRAMES       MB[2]    /* frames in this utterance */
#define MB_PHONES       MB[3]    /* const uint8_t *, frames  */
#define MB_STRESS       MB[4]    /* const uint8_t *, frames  */
#define MB_WORDIDS      MB[5]    /* const uint16_t *, frames */
#define MB_PROSODY_ARENA MB[6]   /* uint8_t *, prosody scratch */
#define MB_PROSODY_BYTES MB[7]
#define MB_FEAT         MB[8]    /* float *, frames * 20     */
#define MB_PCM          MB[9]    /* int16_t *, frames * 160  */
#define MB_STATUS       MB[10]   /* V5F result code          */
#define MB_MS           MB[11]   /* V5F synthesis time, ms   */
#define MB_JOB          MB[12]   /* V3F increments per job   */
#define MB_DONE         MB[13]   /* V5F echoes the job id    */
#define MB_STAGE        MB[14]   /* V5F init progress, 1 to 4 */

#define TTS_CMD_IDLE    0u
#define TTS_CMD_SYNTH   1u
#define TTS_CMD_INIT    2u   /* bind the weights, V5F does it in app_run */

#define TTS_READY_MAGIC 0xCAFEu

#define TTS_OK             0u
#define TTS_ERR_FRAMES     1u   /* frame count over the cap */
#define TTS_ERR_ARENA      2u   /* prosody arena too small  */
#define TTS_ERR_ACOUSTIC   3u
#define TTS_ERR_PROSODY    4u
#define TTS_ERR_WEIGHTS    5u

/* Longest utterance. Sizes the PCM buffer and the prosody arena. */
#define TTS_MAX_FRAMES  250u

#define FENCE()  __asm volatile("fence rw,rw" ::: "memory")

#endif /* TTS_MAILBOX_H */
