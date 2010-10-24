#pragma once
#ifndef BASIC_H_ZPM7ZK97
#define BASIC_H_ZPM7ZK97

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

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
static const size_t SN_MEMORY_PAGE_SIZE = 4096; // XXX: x86 default.

#define QUOTEME_(X) #X
#define QUOTEME(X) QUOTEME_(X)
#define TRAP() do{fprintf(stderr, "TRAP AT %s:%d (%s)!\n", __FILE__, __LINE__, __func__); __builtin_abort();}while(0)
#define ASSERT(x) do {if (!(x)) {fprintf(stderr, "ASSERTION FAILED: %s\n", #x); TRAP();};}while(0)

struct SnProcess;
#define SN_P struct SnProcess*


#endif /* end of include guard: BASIC_H_ZPM7ZK97 */
