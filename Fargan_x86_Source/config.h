/*
 * config.h
 *
 * Portable build configuration cut down from the Opus autoconf output.
 * Every x86 and ARM SIMD path is off.
 */

#ifndef CONFIG_H
#define CONFIG_H

#define DISABLE_DEBUG_FLOAT 1

/* nnet.c needs this for MAX_RNN_NEURONS_ALL */
#define ENABLE_DEEP_PLC 1

/* #undef ENABLE_DRED */
/* #undef ENABLE_OSCE */
/* #undef ENABLE_OSCE_BWE */
/* #undef ENABLE_LOSSGEN */

#define ENABLE_HARDENING 1

/* Float, not fixed point */
/* #undef DISABLE_FLOAT_API */
/* #undef FIXED_POINT */

/* No x86 SIMD */
/* #undef OPUS_X86_MAY_HAVE_SSE */
/* #undef OPUS_X86_MAY_HAVE_SSE2 */
/* #undef OPUS_X86_MAY_HAVE_SSE4_1 */
/* #undef OPUS_X86_MAY_HAVE_AVX2 */
/* #undef OPUS_X86_PRESUME_SSE */
/* #undef OPUS_X86_PRESUME_SSE2 */
/* #undef OPUS_X86_PRESUME_SSE4_1 */
/* #undef OPUS_X86_PRESUME_AVX2 */
/* #undef CPU_INFO_BY_ASM */

/* No ARM NEON */
/* #undef OPUS_ARM_ASM */
/* #undef OPUS_ARM_INLINE_ASM */
/* #undef OPUS_ARM_INLINE_EDSP */
/* #undef OPUS_ARM_INLINE_MEDIA */
/* #undef OPUS_ARM_INLINE_NEON */
/* #undef OPUS_ARM_MAY_HAVE_DOTPROD */
/* #undef OPUS_ARM_MAY_HAVE_EDSP */
/* #undef OPUS_ARM_MAY_HAVE_MEDIA */
/* #undef OPUS_ARM_MAY_HAVE_NEON */
/* #undef OPUS_ARM_MAY_HAVE_NEON_INTR */
/* #undef OPUS_ARM_PRESUME_AARCH64_NEON_INTR */
/* #undef OPUS_ARM_PRESUME_DOTPROD */
/* #undef OPUS_ARM_PRESUME_EDSP */
/* #undef OPUS_ARM_PRESUME_MEDIA */
/* #undef OPUS_ARM_PRESUME_NEON */
/* #undef OPUS_ARM_PRESUME_NEON_INTR */

/* No runtime CPU detection */
/* #undef OPUS_HAVE_RTCD */

#define SUPPRESS_PERF_WARNINGS 1

#define PACKAGE_VERSION "tts-1.0"

#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SHORT 2

#define VAR_ARRAYS 1

#if defined(__GNUC__) || defined(__clang__)
#define restrict __restrict__
#elif defined(_MSC_VER)
#define restrict __restrict
#endif

#endif /* CONFIG_H */
