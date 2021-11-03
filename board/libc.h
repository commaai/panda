// **** libc ****

void delay(uint32_t a) {
  volatile uint32_t i;
  for (i = 0; i < a; i++);
}

void *memset(void *str, int c, unsigned int n) {
  uint8_t *s = str;
  for (unsigned int i = 0; i < n; i++) {
    *s = c;
    s++;
  }
  return str;
}

void *memcpy(void *dest, const void *src, unsigned int n) {
  const uint32_t *s32 = (const uint32_t *)src;
  uint32_t *d32 = (uint32_t *)dest;
  for (unsigned int i = (n / 4U); i > 0U; i--) {
    *d32 = *s32;
    d32++;
    s32++;
  }

  const uint8_t *s8 = (const uint8_t *)s32;
  uint8_t *d8 = (uint8_t *)d32;
  for (unsigned int i = (n & 0x03U); i > 0U; i--) {
    *d8 = *s8;
    d8++;
    s8++;
  }
  return dest;
}

int memcmp(const void * ptr1, const void * ptr2, unsigned int num) {
  int ret = 0;
  const uint8_t *p1 = ptr1;
  const uint8_t *p2 = ptr2;
  for (unsigned int i = 0; i < num; i++) {
    if (*p1 != *p2) {
      ret = -1;
      break;
    }
    p1++;
    p2++;
  }
  return ret;
}
