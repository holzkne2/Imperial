#pragma once

#include <stdint.h>

#define PI32 3.14159265359f

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float  real32;
typedef double real64;

real32 remap_n11(const real32 low, const real32 high, const real32 value);