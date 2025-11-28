#pragma once

#include <stdint.h>
#include <stdbool.h>

// **** libc ****

#define UNALIGNED(X, Y) \
  (((uint32_t)(X) & (sizeof(uint32_t) - 1U)) | ((uint32_t)(Y) & (sizeof(uint32_t) - 1U)))

void delay(uint32_t a);
void assert_fatal(bool condition, const char *msg);
void *memset(void *str, int c, unsigned int n);
void *memcpy(void *dest, const void *src, unsigned int len);
int memcmp(const void * ptr1, const void * ptr2, unsigned int num);
void print(const char *a);
void puth(unsigned int i);