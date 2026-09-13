/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2025 Rovari, rvembedded.com
 *
 * tts_provision.c
 *
 * Copies the weight blobs from the SD card into CodeFlash on first
 * boot. Uses FLASH_ROM_ERASE and FLASH_ROM_WRITE, which take the fast
 * page path and accept bank 1 addresses. Erase is aligned to 8192,
 * write to 256. Everything runs from RAM_CODE, so do not move .rodata
 * out of .highcode.
 */

#include "rovari.h"
#include "rovari_sdcard_spi.h"
#include "tts_provision.h"

#include "ch32h417_flash.h"
#include "ff.h"

#include <string.h>
#include <stdio.h>

#ifndef TTS_PROV_LOG
#define TTS_PROV_LOG(...)   printf(__VA_ARGS__)
#endif

#define TTS_PROV_MAGIC      0x54565235u   /* "5RVT" little endian */
#define TTS_PROV_VERSION    5u

#define ERASE_GRAIN         8192u
#define PROGRAM_GRAIN       256u
#define PROV_CHUNK          4096u

#define ROUND_UP(v, g)      ((((v) + (g) - 1u) / (g)) * (g))
#define ERASE_END(a, l)     ((a) + ROUND_UP((l), ERASE_GRAIN))

typedef struct {
    uint32_t addr;
    uint32_t len;
    uint32_t crc;
} tts_blob_record_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t reserved;
    tts_blob_record_t blob[TTS_BLOB_COUNT];
} tts_manifest_t;

typedef struct {
    uint32_t    addr;
    uint32_t    len;
    const char *path;
    const char *name;
} tts_blob_desc_t;

/* Address order, so the manifest reads the way the map does. */
static const tts_blob_desc_t g_blobs[TTS_BLOB_COUNT] = {
    { TTS_PROSODY_ADDR,  TTS_PROSODY_LEN,  TTS_PROSODY_PATH,  "prosody"  },
    { TTS_VOCAB_ADDR,    TTS_VOCAB_LEN,    TTS_VOCAB_PATH,    "vocab"    },
    { TTS_ACOUSTIC_ADDR, TTS_ACOUSTIC_LEN, TTS_ACOUSTIC_PATH, "acoustic" }
};

static uint8_t g_buf[PROV_CHUNK] __attribute__((aligned(4)));

/* Layout errors caught at compile time, in address order. */
#if (TTS_V5F_CODE_ADDR != 0x00040000u)
#error "the V5F image must stay at the stock 0x00040000; move the blobs instead"
#endif

#if (TTS_PROSODY_ADDR % ERASE_GRAIN) || (TTS_VOCAB_ADDR % ERASE_GRAIN) \
 || (TTS_MANIFEST_ADDR % ERASE_GRAIN) || (TTS_ACOUSTIC_ADDR % ERASE_GRAIN)
#error "every blob start must be on an 8192 byte erase boundary"
#endif

#if TTS_PROSODY_ADDR < TTS_V3F_CODE_END
#error "prosody starts inside the V3F image"
#endif

#if ERASE_END(TTS_PROSODY_ADDR, TTS_PROSODY_LEN) > TTS_V5F_CODE_ADDR
#error "prosody erase span reaches into the V5F image"
#endif

#if TTS_VOCAB_ADDR < TTS_V5F_CODE_END
#error "vocab starts inside the V5F image"
#endif

#if ERASE_END(TTS_VOCAB_ADDR, TTS_VOCAB_LEN) > TTS_BANK1_BASE
#error "vocab erase span crosses the bank boundary"
#endif

#if TTS_ACOUSTIC_ADDR < TTS_BANK1_BASE
#error "acoustic must start at or above the bank boundary"
#endif

#if ERASE_END(TTS_ACOUSTIC_ADDR, TTS_ACOUSTIC_LEN) > TTS_MANIFEST_ADDR
#error "acoustic erase span reaches into the manifest page"
#endif

#if (TTS_MANIFEST_ADDR + ERASE_GRAIN) > TTS_FLASH_END
#error "manifest page is past the end of programmable flash"
#endif

static uint32_t crc32_flash(uint32_t addr, uint32_t len)
{
    const uint32_t *w = (const uint32_t *)addr;
    uint32_t nwords = len >> 2;
    uint32_t tail   = len & 3u;
    uint32_t i;

    crc_reset();

    for (i = 0; i < nwords; i++) {
        crc_feed(w[i]);
    }

    if (tail != 0u) {
        const uint8_t *b = (const uint8_t *)(addr + (nwords << 2));
        uint32_t last = 0u;
        for (i = 0; i < tail; i++) {
            last |= ((uint32_t)b[i]) << (8u * i);
        }
        crc_feed(last);
    }

    return crc_get();
}

static tts_prov_status_t provision_blob(const tts_blob_desc_t *d)
{
    FIL      fil;
    UINT     br;
    uint32_t off = 0u;
    uint32_t t0;

    if (f_open(&fil, d->path, FA_READ) != FR_OK) {
        TTS_PROV_LOG("prov: cannot open %s\r\n", d->path);
        return TTS_PROV_ERR_OPEN;
    }

    if ((uint32_t)f_size(&fil) != d->len) {
        TTS_PROV_LOG("prov: %s is %lu bytes, expected %lu\r\n",
                     d->path, (unsigned long)f_size(&fil),
                     (unsigned long)d->len);
        f_close(&fil);
        return TTS_PROV_ERR_SIZE;
    }

    t0 = millis();

    if (FLASH_ROM_ERASE(TTS_ALIAS(d->addr), ROUND_UP(d->len, ERASE_GRAIN))
        != FLASH_COMPLETE) {
        TTS_PROV_LOG("prov: erase failed at 0x%08lX\r\n",
                     (unsigned long)d->addr);
        f_close(&fil);
        return TTS_PROV_ERR_ERASE;
    }

    while (off < d->len) {
        uint32_t want  = d->len - off;
        uint32_t chunk;
        uint32_t i;

        if (want > PROV_CHUNK) {
            want = PROV_CHUNK;
        }

        if (f_read(&fil, g_buf, want, &br) != FR_OK || br != want) {
            f_close(&fil);
            return TTS_PROV_ERR_READ;
        }

        chunk = ROUND_UP(want, PROGRAM_GRAIN);
        for (i = want; i < chunk; i++) {
            g_buf[i] = 0xFFu;
        }

        if (FLASH_ROM_WRITE(TTS_ALIAS(d->addr + off),
                            (uint32_t *)g_buf, chunk) != FLASH_COMPLETE) {
            TTS_PROV_LOG("prov: write failed at 0x%08lX\r\n",
                         (unsigned long)(d->addr + off));
            f_close(&fil);
            return TTS_PROV_ERR_WRITE;
        }

        if (memcmp((const void *)(d->addr + off), g_buf, chunk) != 0) {
            TTS_PROV_LOG("prov: verify failed at 0x%08lX\r\n",
                         (unsigned long)(d->addr + off));
            f_close(&fil);
            return TTS_PROV_ERR_VERIFY;
        }

        off += want;
    }

    f_close(&fil);

    TTS_PROV_LOG("prov: %s %lu bytes -> 0x%08lX in %lu ms\r\n",
                 d->name, (unsigned long)d->len,
                 (unsigned long)d->addr, (unsigned long)(millis() - t0));

    return TTS_PROV_OK;
}

static tts_prov_status_t write_manifest(void)
{
    tts_manifest_t *m = (tts_manifest_t *)g_buf;
    uint32_t i;

    if (FLASH_ROM_ERASE(TTS_ALIAS(TTS_MANIFEST_ADDR), ERASE_GRAIN)
        != FLASH_COMPLETE) {
        return TTS_PROV_ERR_ERASE;
    }

    memset(g_buf, 0xFF, PROGRAM_GRAIN);

    m->magic    = TTS_PROV_MAGIC;
    m->version  = TTS_PROV_VERSION;
    m->count    = TTS_BLOB_COUNT;
    m->reserved = 0u;

    for (i = 0u; i < TTS_BLOB_COUNT; i++) {
        m->blob[i].addr = g_blobs[i].addr;
        m->blob[i].len  = g_blobs[i].len;
        m->blob[i].crc  = crc32_flash(g_blobs[i].addr, g_blobs[i].len);
    }

    if (FLASH_ROM_WRITE(TTS_ALIAS(TTS_MANIFEST_ADDR),
                        (uint32_t *)g_buf, PROGRAM_GRAIN) != FLASH_COMPLETE) {
        return TTS_PROV_ERR_MANIFEST;
    }

    if (memcmp((const void *)TTS_MANIFEST_ADDR, g_buf, PROGRAM_GRAIN) != 0) {
        return TTS_PROV_ERR_MANIFEST;
    }

    return TTS_PROV_OK;
}

static int manifest_valid(void)
{
    const tts_manifest_t *m = (const tts_manifest_t *)TTS_MANIFEST_ADDR;
    uint32_t i;

    if (m->magic != TTS_PROV_MAGIC) {
        return 0;
    }
    if (m->version != TTS_PROV_VERSION || m->count != TTS_BLOB_COUNT) {
        return 0;
    }

    for (i = 0u; i < TTS_BLOB_COUNT; i++) {
        if (m->blob[i].addr != g_blobs[i].addr) {
            return 0;
        }
        if (m->blob[i].len != g_blobs[i].len) {
            return 0;
        }
        if (m->blob[i].crc != crc32_flash(g_blobs[i].addr, g_blobs[i].len)) {
            TTS_PROV_LOG("prov: %s CRC mismatch\r\n", g_blobs[i].name);
            return 0;
        }
    }

    return 1;
}

tts_prov_status_t tts_provision_ensure(void)
{
    tts_prov_status_t st;
    uint32_t i;

    crc_init();

    if (manifest_valid()) {
        TTS_PROV_LOG("prov: manifest OK, blobs already resident\r\n");
        return TTS_PROV_ALREADY;
    }

    TTS_PROV_LOG("prov: provisioning from card\r\n");

    if (sd_init() != SD_OK) {
        TTS_PROV_LOG("prov: SD mount failed\r\n");
        return TTS_PROV_ERR_SD;
    }

    for (i = 0u; i < TTS_BLOB_COUNT; i++) {
        st = provision_blob(&g_blobs[i]);
        if (st != TTS_PROV_OK) {
            return st;
        }
    }

    st = write_manifest();
    if (st != TTS_PROV_OK) {
        TTS_PROV_LOG("prov: manifest write failed\r\n");
        return st;
    }

    TTS_PROV_LOG("prov: complete\r\n");
    return TTS_PROV_OK;
}

tts_prov_status_t tts_provision_invalidate(void)
{
    if (FLASH_ROM_ERASE(TTS_ALIAS(TTS_MANIFEST_ADDR), ERASE_GRAIN)
        != FLASH_COMPLETE) {
        return TTS_PROV_ERR_ERASE;
    }
    return TTS_PROV_OK;
}

const char *tts_provision_strerror(tts_prov_status_t s)
{
    switch (s) {
    case TTS_PROV_OK:           return "ok";
    case TTS_PROV_ALREADY:      return "already provisioned";
    case TTS_PROV_ERR_SD:       return "sd mount failed";
    case TTS_PROV_ERR_OPEN:     return "file open failed";
    case TTS_PROV_ERR_SIZE:     return "file size mismatch";
    case TTS_PROV_ERR_READ:     return "file read failed";
    case TTS_PROV_ERR_ERASE:    return "flash erase failed";
    case TTS_PROV_ERR_WRITE:    return "flash write failed";
    case TTS_PROV_ERR_VERIFY:   return "flash verify failed";
    case TTS_PROV_ERR_MANIFEST: return "manifest write failed";
    default:                    return "unknown";
    }
}
