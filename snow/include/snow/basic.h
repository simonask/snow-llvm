#pragma once
#ifndef BASIC_H_ZPM7ZK97
#define BASIC_H_ZPM7ZK97

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#define NO_INLINE __attribute__((noinline))
#define HIDDEN __attribute__((visibility("hidden")))
#define SENTINEL __attribute__((sentinel))

#ifndef byte
typedef unsigned char byte;
#endif

#ifdef __cplusplus
#define CAPI extern "C"
#if !defined(INLINE)
#define INLINE inline
#endif
#else
#define CAPI 
#if !defined(INLINE)
#define INLINE static inline
#endif
#endif

CAPI void abort();

#define LIKELY(X) __builtin_expect(X, true)
#define UNLIKELY(X) __builtin_expect(X, false)

static const size_t SN_CACHE_LINE_SIZE = 64; // XXX: x86 default.
static const size_t SN_L2_CACHE_SIZE = 1024 * 2048; // XXX: 2 MiB default
static const size_t SN_OBJECT_SIZE = SN_CACHE_LINE_SIZE;
static const size_t SN_MEMORY_PAGE_SIZE = 4096; // XXX: x86 default.
static const size_t SN_ALLOCATION_BLOCK_SIZE = 4 * SN_MEMORY_PAGE_SIZE; // 16K default

#define QUOTEME_(X) #X
#define QUOTEME(X) QUOTEME_(X)

#if defined(DEBUG)
INLINE void crash_and_burn() { assert(false); }
#endif

#if defined(DEBUG)
#define TRAP() do { fprintf(stderr, "TRAP AT %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); __asm__("int3\n"); } while (0)
#else
#define TRAP() __builtin_unreachable()
#endif

#if defined(DEBUG)
#define ASSERT(x) do { if (!(x)) { fprintf(stderr, "ASSERTION FAILED: %s\n", #x); TRAP(); } } while (0)
#else
#define ASSERT(x)
#endif

#endif /* end of include guard: BASIC_H_ZPM7ZK97 */
