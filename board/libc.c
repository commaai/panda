#include "board/config.h"
#include "board/utils.h"
#include "board/libc.h"

#ifdef STM32H7

void *memset(void *str, int c, size_t n) {
  uint8_t *s = (uint8_t *)str;
  for (size_t i = 0; i < n; i++) {
    s[i] = (uint8_t)c;
  }
  return str;
}

void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  int ret = 0;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      ret = (p1[i] < p2[i]) ? -1 : 1;
      break;
    }
  }
  return ret;
}

#endif
