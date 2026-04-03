#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

#define MIN(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a < _b) ? _a : _b; \
})

#define MAX(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a > _b) ? _a : _b; \
})

#define CLAMP(x, low, high) ({ \
  __typeof__ (x) _x = (x); \
  __typeof__ (low) _low = (low); \
  __typeof__ (high) _high = (high); \
  (_x < _low) ? _low : ((_x > _high) ? _high : _x); \
})

#define ABS(a) ({ \
  __typeof__ (a) _a = (a); \
  (_a < 0) ? -_a : _a; \
})

#define UNALIGNED(ptr1, ptr2) (((uint32_t)(ptr1) | (uint32_t)(ptr2)) & 3U)

#define COMPILE_TIME_ASSERT(condition) ((void)sizeof(char[1 - (2 * ((condition) ? 0 : 1))]))

// Use static inline for delay to avoid linker issues and ensure it's available in all translation units
static inline void delay(uint32_t a) {
  volatile uint32_t i;
  uint32_t n = a * 13U / 5U;
  for (i = 0; i < n; i++) {
    __asm__("nop");
  }
}

uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last);

#endif
