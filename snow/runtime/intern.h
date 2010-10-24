#pragma once
#ifndef INTERN_H_8A43ZRK3
#define INTERN_H_8A43ZRK3

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) snow_printf(fmt, __VA_ARGS__, NULL)
#else
#define DEBUG_PRINT(...) 
#endif

#endif /* end of include guard: INTERN_H_8A43ZRK3 */
