/*
 * tts_compat.h
 *
 * Everything the pipeline needs from libc and libm, so the target
 * build depends on neither. Freestanding by default because the
 * embedded build cannot pass a -D flag and real printf drags in
 * libc_nano. Define TTS_HOSTED for a PC build.
 */

#ifndef TTS_COMPAT_H
#define TTS_COMPAT_H

#include <stdint.h>
#include <stddef.h>

/* All C, so the header is safe in a C++ unit in either include order. */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef TTS_HOSTED      /* freestanding by default */

/* bao_stdlib lacks memmove, strlen, strcmp, strncmp and strncpy. */
void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);
char *strchr(const char *, int);

void *memmove(void *, const void *, size_t);   /* tts_stdlib.c */
static inline size_t tts_strlen_(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}
static inline int tts_strcmp_(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
static inline int tts_strncmp_(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
static inline char *tts_strncpy_(char *d, const char *s, size_t n)
{
    size_t i = 0;
    while (i < n && s[i]) { d[i] = s[i]; i++; }
    while (i < n) d[i++] = 0;
    return d;
}

#define strlen  tts_strlen_
#define strcmp  tts_strcmp_
#define strncmp tts_strncmp_
#define strncpy tts_strncpy_

#define tts_isdigit(c) ((c) >= '0' && (c) <= '9')
#define tts_isalpha(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define tts_isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')
#define tts_toupper(c) (((c) >= 'a' && (c) <= 'z') ? (char)((c) - 32) : (c))
#define tts_tolower(c) (((c) >= 'A' && (c) <= 'Z') ? (char)((c) + 32) : (c))

/* base 2 exponential from the float exponent field, as Opus does */
static inline float tts_exp2f(float x)
{
    union { float f; uint32_t i; } r;
    int n = (int)(x >= 0.0f ? x : x - 1.0f);
    float f = x - (float)n;
    if (n < -60) return 0.0f;
    if (n > 60) n = 60;
    r.f = 0.99992522f + f * (0.69583354f + f * (0.22606716f + 0.078024523f * f));
    r.i = (uint32_t)((int32_t)r.i + (n << 23));
    return r.f;
}

static inline float tts_expf(float x) { return tts_exp2f(x * 1.44269504f); }

/* log2 from the exponent field plus an atanh series, good to 2e-6. */
static inline float tts_log2f(float x)
{
    union { float f; uint32_t i; } v;
    float m, z, z2;
    int e;
    if (x <= 0.0f) return -60.0f;
    v.f = x;
    e = (int)((v.i >> 23) & 0xFFu) - 127;
    v.i = (v.i & 0x007FFFFFu) | 0x3F800000u;
    m = v.f;
    if (m > 1.41421356f) { m *= 0.5f; e++; }
    z = (m - 1.0f) / (m + 1.0f);
    z2 = z * z;
    return 2.88539008f * z * (1.0f + z2 * (0.33333333f + z2 * 0.2f))
         + (float)e;
}

static inline float tts_powf(float b, float e)
{
    if (b <= 0.0f) return 0.0f;
    return tts_exp2f(e * tts_log2f(b));
}

static inline float tts_logf(float x)   { return tts_log2f(x) * 0.69314718f; }
static inline float tts_log10f(float x) { return tts_log2f(x) * 0.30103000f; }

static inline float tts_sqrtf(float x)
{
    union { float f; uint32_t i; } u;
    float y;
    if (x <= 0.0f) return 0.0f;
    u.f = x;
    u.i = 0x1fbd1df5u + (u.i >> 1);
    y = u.f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

/* tanh through the exponential, which the approximation above makes
   accurate to about 1e-4 across the whole useful range */
static inline float tts_tanhf(float x)
{
    float e;
    if (x > 9.0f) return 1.0f;
    if (x < -9.0f) return -1.0f;
    e = tts_exp2f(x * 2.88539008f);        /* exp(2x) */
    return (e - 1.0f) / (e + 1.0f);
}

/* range reduce to [-pi, pi] then a degree 10 even polynomial */
static inline float tts_cosf(float x)
{
    float x2;
    const float inv2pi = 0.15915494f, twopi = 6.28318531f;
    int k = (int)(x * inv2pi + (x >= 0.0f ? 0.5f : -0.5f));
    x -= (float)k * twopi;
    x2 = x * x;
    return 0.999999444f + x2 * (-0.499995581f + x2 * (0.041661033f
         + x2 * (-0.001386275f + x2 * (0.000024253f + x2 * -0.000000222f))));
}

#define tts_exp(x)   ((double)tts_expf((float)(x)))

#else /* TTS_HOSTED, PC build */

#include <string.h>
#include <ctype.h>
#include <math.h>

#define tts_isdigit isdigit
#define tts_isalpha isalpha
#define tts_isspace isspace
#define tts_toupper toupper
#define tts_tolower tolower
#define tts_expf    expf
#define tts_powf    powf
#define tts_logf    logf
#define tts_log10f  log10f
#define tts_sqrtf   sqrtf
#define tts_tanhf   tanhf
#define tts_cosf    cosf
#define tts_exp     exp

#endif

/* Diagnostics. Point TTS_LOG at mini_printf on the target, or define
   TTS_QUIET to remove them entirely. */
#if defined(TTS_QUIET)
#define TTS_LOG(...) ((void)0)
#elif defined(TTS_HOSTED)
#include <stdio.h>
#define TTS_LOG(...) printf(__VA_ARGS__)
#else
/* matches bao/stdlib.h exactly: it returns void, not int */
void tts_log(const char *, ...);
#define TTS_LOG(...) tts_log(__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* TTS_COMPAT_H */
