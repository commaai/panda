#pragma once

#include <stdint.h>
#include <stdbool.h>

// cppcheck-suppress-macro [misra-c2012-1.2, misra-c2012-17.3]; allow __typeof__ extension
#define MIN(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a < _b) ? _a : _b; \
})

// cppcheck-suppress-macro [misra-c2012-1.2, misra-c2012-17.3]; allow __typeof__ extension
#define MAX(a, b) ({ \
  __typeof__ (a) _a = (a); \
  __typeof__ (b) _b = (b); \
  (_a > _b) ? _a : _b; \
})

// cppcheck-suppress-macro [misra-c2012-1.2, misra-c2012-17.3]; allow __typeof__ extension
#define CLAMP(x, low, high) ({ \
  __typeof__(x) __x = (x); \
  __typeof__(low) __low = (low);\
  __typeof__(high) __high = (high);\
  (__x > __high) ? __high : ((__x < __low) ? __low : __x); \
})

// cppcheck-suppress-macro [misra-c2012-1.2, misra-c2012-17.3]; allow __typeof__ extension
#define ABS(a) ({ \
  __typeof__ (a) _a = (a); \
  (_a > 0) ? _a : (-_a); \
})

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

uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last);
void delay(uint32_t a);
void assert_fatal(bool condition, const char *msg);
// cppcheck-suppress misra-c2012-21.2
void *memset(void *str, int c, unsigned int n);
// cppcheck-suppress misra-c2012-21.2
void *memcpy(void *dest, const void *src, unsigned int len);
// cppcheck-suppress misra-c2012-21.2
int memcmp(const void *ptr1, const void *ptr2, unsigned int num);
uint8_t crc_checksum(const uint8_t *dat, int len, uint8_t poly);
