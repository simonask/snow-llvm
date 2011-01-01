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

#define NO_INLINE __attribute__((noinline))
#define HIDDEN __attribute__((visibility("hidden")))
#define SENTINEL __attribute__((sentinel))

#ifndef byte
typedef unsigned char byte;
#endif

#ifdef __cplusplus
#define CAPI extern "C"
#else
#define CAPI 
#endif

static const size_t SN_CACHE_LINE_SIZE = 64; // XXX: x86 default.
static const size_t SN_OBJECT_MAX_SIZE = SN_CACHE_LINE_SIZE;
static const size_t SN_MEMORY_PAGE_SIZE = 4096; // XXX: x86 default.
static const size_t SN_MALLOC_OVERHEAD = 8; // XXX: guess...

#define QUOTEME_(X) #X
#define QUOTEME(X) QUOTEME_(X)

#if defined(DEBUG)
static inline void crash_and_burn() { __asm__(""); char c = *(char*)NULL; }
#endif

#if defined(DEBUG)
#define TRAP() do { fprintf(stderr, "TRAP AT %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__); crash_and_burn(); } while (0)
#else
#define TRAP() __builtin_abort()
#endif

#if defined(DEBUG)
#define ASSERT(x) do { if (!(x)) { fprintf(stderr, "ASSERTION FAILED: %s\n", #x); TRAP(); } } while (0)
#else
#define ASSERT(x) do { if (!(x)) { fprintf(stderr, "ASSERTION FAILED at %s:%d (): %s\n", __FILE__, __LINE__, __FUNCTION__, #x); TRAP(); } } while (0)
#endif

struct SnProcess;
#define SN_P struct SnProcess*

#endif /* end of include guard: BASIC_H_ZPM7ZK97 */
