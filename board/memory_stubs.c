// Memory and USB stub implementations for all builds
// This file provides the missing symbols that cause linking errors

#include <stdint.h>
#include <stdbool.h>

// Memory functions - needed when libc.h static inline doesn't link properly
void *memset(void *s, int c, unsigned int n) {
  unsigned char *p = s;
  while(n--) *p++ = (unsigned char)c;
  return s;
}

void *memcpy(void *dest, const void *src, unsigned int n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  while(n--) *d++ = *s++;
  return dest;
}

// USB initialization stub - platform independent
void usb_init(void) {
  // Empty implementation
}