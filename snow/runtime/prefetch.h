#pragma once
#ifndef PREFETCH_H_9B6R0HEX
#define PREFETCH_H_9B6R0HEX

#include "snow/basic.h"

typedef enum SnPrefetchType {
	SnPrefetchRead      = 0,
	SnPrefetchReadWrite = 1
} SnPrefetchType;

typedef enum SnPrefetchLocality {
	SnPrefetchLocalityNone   = 0,
	SnPrefetchLocalityLow    = 1,
	SnPrefetchLocalityMedium = 2,
	SnPrefetchLocalityHigh   = 3
} SnPrefetchLocality;

#if defined(__builting_prefetch)
#define snow_prefetch(ADDR, PREFETCH_TYPE, PREFETCH_LOCALITY) __builtin_prefetch(ADDR, PREFETCH_TYPE, PREFETCH_LOCALITY)
#else
#define snow_prefetch(ADDR, PT, PL)
#endif

#endif /* end of include guard: PREFETCH_H_9B6R0HEX */
