#pragma once

#include <stddef.h>

static inline int min_int(int a, int b) {
  return (a < b) ? a : b;
}

static inline unsigned int min_uint(unsigned int a, unsigned int b) {
  return (a < b) ? a : b;
}

static inline long min_long(long a, long b) {
  return (a < b) ? a : b;
}

static inline unsigned long min_ulong(unsigned long a, unsigned long b) {
  return (a < b) ? a : b;
}

static inline long long min_llong(long long a, long long b) {
  return (a < b) ? a : b;
}

static inline unsigned long long min_ullong(unsigned long long a, unsigned long long b) {
  return (a < b) ? a : b;
}

static inline float min_float(float a, float b) {
  return (a < b) ? a : b;
}

static inline double min_double(double a, double b) {
  return (a < b) ? a : b;
}

static inline long double min_ldouble(long double a, long double b) {
  return (a < b) ? a : b;
}

static inline int max_int(int a, int b) {
  return (a > b) ? a : b;
}

static inline unsigned int max_uint(unsigned int a, unsigned int b) {
  return (a > b) ? a : b;
}

static inline long max_long(long a, long b) {
  return (a > b) ? a : b;
}

static inline unsigned long max_ulong(unsigned long a, unsigned long b) {
  return (a > b) ? a : b;
}

static inline long long max_llong(long long a, long long b) {
  return (a > b) ? a : b;
}

static inline unsigned long long max_ullong(unsigned long long a, unsigned long long b) {
  return (a > b) ? a : b;
}

static inline float max_float(float a, float b) {
  return (a > b) ? a : b;
}

static inline double max_double(double a, double b) {
  return (a > b) ? a : b;
}

static inline long double max_ldouble(long double a, long double b) {
  return (a > b) ? a : b;
}

static inline int clamp_int(int x, int low, int high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline unsigned int clamp_uint(unsigned int x, unsigned int low, unsigned int high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline long clamp_long(long x, long low, long high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline unsigned long clamp_ulong(unsigned long x, unsigned long low, unsigned long high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline long long clamp_llong(long long x, long long low, long long high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline unsigned long long clamp_ullong(unsigned long long x, unsigned long long low, unsigned long long high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline float clamp_float(float x, float low, float high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline double clamp_double(double x, double low, double high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline long double clamp_ldouble(long double x, long double low, long double high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline int abs_int(int a) {
  return (a > 0) ? a : -a;
}

static inline unsigned int abs_uint(unsigned int a) {
  return a;
}

static inline long abs_long(long a) {
  return (a > 0L) ? a : -a;
}

static inline unsigned long abs_ulong(unsigned long a) {
  return a;
}

static inline long long abs_llong(long long a) {
  return (a > 0LL) ? a : -a;
}

static inline unsigned long long abs_ullong(unsigned long long a) {
  return a;
}

static inline float abs_float(float a) {
  return (a > 0.0f) ? a : -a;
}

static inline double abs_double(double a) {
  return (a > 0.0) ? a : -a;
}

static inline long double abs_ldouble(long double a) {
  return (a > 0.0L) ? a : -a;
}

// The selector only determines the promoted type; _Generic does not evaluate it.
// cppcheck-suppress-macro misra-c2012-1.4 ; C11 generic selection keeps arguments evaluated once
#define MATH_FUNC(value, name) _Generic((value), \
  int: (name##_int), unsigned int: (name##_uint), \
  long: (name##_long), unsigned long: (name##_ulong), \
  long long: (name##_llong), unsigned long long: (name##_ullong), \
  float: (name##_float), double: (name##_double), long double: (name##_ldouble))

#define MIN(a, b) MATH_FUNC((a) + (b), min)((a), (b))
#define MAX(a, b) MATH_FUNC((a) + (b), max)((a), (b))
#define CLAMP(x, low, high) MATH_FUNC((x) + (low) + (high), clamp)((x), (low), (high))
#define ABS(a) MATH_FUNC(+(a), abs)((a))

// STM32 HAL defines this
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * (!(pred) ? 1 : 0))]))

// compute the time elapsed (in microseconds) from 2 counter samples
// case where ts < ts_last is ok: overflow is properly re-casted into uint32_t
uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  return ts - ts_last;
}
