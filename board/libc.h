#ifndef PANDA_LIBC_H
#define PANDA_LIBC_H

#define min(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   _a < _b ? _a : _b; })

#define max(a,b) \
 ({ __typeof__ (a) _a = (a); \
     __typeof__ (b) _b = (b); \
   _a > _b ? _a : _b; })

// **** shitty libc ****

void clock_init();

void delay(int a);

void *memset(void *str, int c, unsigned int n);

void *memcpy(void *dest, const void *src, unsigned int n);

int memcmp(const void * ptr1, const void * ptr2, unsigned int num);

#endif
