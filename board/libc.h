#pragma once
#include <stdint.h>
#include <stdbool.h>
// **** libc ****

void delay(uint32_t a);

void assert_fatal(bool condition, const char *msg);
// cppcheck-suppress misra-c2012-21.2
void *memset(void *str, int c, unsigned int n);

// cppcheck-suppress misra-c2012-21.2
void *memcpy(void *dest, const void *src, unsigned int len);

// cppcheck-suppress misra-c2012-21.2
int memcmp(const void * ptr1, const void * ptr2, unsigned int num);