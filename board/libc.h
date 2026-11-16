#ifndef BOARD_LIBC_H
#define BOARD_LIBC_H

#include "board/utils.h"

// **** libc ****

// cppcheck-suppress misra-c2012-1.2 ; no inlining keeps the busy loop timing constant
FLASH_LINE_ALIGNED __attribute__((noinline)) static void delay(uint32_t a) {
  // loop is 2.6x faster when 32-byte aligned (ART accelerator prefetches flash in 32-byte chunks)
  uint32_t n = a * 13U / 5U;
  volatile uint32_t i = 0U;
  while (i < n) {
    i++;
  }
}

static inline void assert_fatal(bool condition, const char *msg) {
  if (!condition) {
    print("ASSERT FAILED\n");
    print(msg);
    while (true) {
      // hang
    }
  }
}

// GCC emits calls to memset and memcpy for struct copies and initializers, even with -fno-builtin.
// They keep the standard names and signatures, so their void pointers become byte and word pointers.
// cppcheck-suppress-begin [misra-c2012-21.2, misra-c2012-11.5] ; the standard functions GCC calls
void *memset(void *str, int c, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int len);

// zeroes every CAN packet from the host
FLASH_LINE_ALIGNED void *memset(void *str, int c, unsigned int n) {
  uint8_t *s = str;
  for (unsigned int i = 0; i < n; i++) {
    *s = (uint8_t)c;
    s++;
  }
  return str;
}

#define UNALIGNED(X, Y) \
  (((uint32_t)(X) & (sizeof(uint32_t) - 1U)) | ((uint32_t)(Y) & (sizeof(uint32_t) - 1U)))

void *memcpy(void *dest, const void *src, unsigned int len) {
  unsigned int n = len;
  uint8_t *d8 = dest;
  const uint8_t *s8 = src;

  if ((n >= 4U) && (UNALIGNED(s8, d8) == 0U)) {
    // already checked that both are word aligned
    uint32_t *d32 = dest;
    const uint32_t *s32 = src;

    while(n >= 16U) {
      *d32 = *s32; d32++; s32++;
      *d32 = *s32; d32++; s32++;
      *d32 = *s32; d32++; s32++;
      *d32 = *s32; d32++; s32++;
      n -= 16U;
    }

    while(n >= 4U) {
      *d32 = *s32; d32++; s32++;
      n -= 4U;
    }

    d8 = (uint8_t *)d32;
    s8 = (const uint8_t *)s32;
  }
  while (n-- > 0U) {
    *d8 = *s8; d8++; s8++;
  }
  return dest;
}
// cppcheck-suppress-end [misra-c2012-21.2, misra-c2012-11.5]

static inline bool bytes_equal(const uint8_t *a, const uint8_t *b, uint32_t len) {
  bool equal = true;
  for (uint32_t i = 0U; (i < len) && equal; i++) {
    equal = (a[i] == b[i]);
  }
  return equal;
}

#endif  // BOARD_LIBC_H
