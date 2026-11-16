#ifndef BOARD_UTILS_H
#define BOARD_UTILS_H

#include <stddef.h>

// places an object in a linker section, for buffers that must live in a RAM region other than the DTCM
// cppcheck-suppress-macro misra-c2012-1.2 ; ISO C has no way to choose the section of an object
#define SECTION(name) __attribute__((section(name)))

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

static inline uint8_t clamp_u8(uint8_t x, uint8_t low, uint8_t high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline int32_t clamp_i32(int32_t x, int32_t low, int32_t high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline float clamp_f32(float x, float low, float high) {
  return (x > high) ? high : ((x < low) ? low : x);
}

static inline int32_t abs_i32(int32_t a) {
  return (a > 0) ? a : -a;
}

static inline float abs_f32(float a) {
  return (a > 0.0f) ? a : -a;
}

// STM32 HAL defines this
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * (!(pred) ? 1 : 0))]))

// compute the time elapsed (in microseconds) from 2 counter samples
// case where ts < ts_last is ok: overflow is properly re-casted into uint32_t
static uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  return ts - ts_last;
}

#endif  // BOARD_UTILS_H
