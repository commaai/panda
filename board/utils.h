static inline unsigned int func_min(unsigned int a, unsigned int b) {
  unsigned int result = 0;
  if (a < b) {
    result = a;
  } else {
    result = b;
  }

  return result;
}

static inline unsigned int func_max(unsigned int a, unsigned int b) {
  unsigned int result = 0;
  if (a > b) {
    result = a;
  } else {
    result = b;
  }
  return result;
}

static inline unsigned int func_clamp(unsigned int a, unsigned int b, unsigned int c) {
  unsigned int result = 0;
  if (a > c) {
    result = c;
  } else if (a < b) {
    result = b;
  } else {
    result = a;
  }

  return result;
}

static inline int func_abs(int a) {
  int result = 0;
  if (a > 0) {
    result = a;
  } else {
    result = -a;
  }
  return result;
}

#define MIN(a, b) \
  (func_min(a, b))

#define MAX(a, b) \
  (func_max(a, b))

#define CLAMP(a, b, c) \
  (func_clamp(a, b, c))

#define ABS(a) \
  (func_abs(a))

#ifndef NULL
// this just provides a standard implementation of NULL
// in lieu of including libc in the panda build
// cppcheck-suppress [misra-c2012-21.1]
#define NULL ((void*)0)
#endif

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
