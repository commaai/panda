#include <stdint.h>
#include "config.h"
#include "libc.h"

void clock_init() {
  // enable external oscillator
  RCC->CR |= RCC_CR_HSEON;
  while ((RCC->CR & RCC_CR_HSERDY) == 0);

  // divide shit
  RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_PPRE1_DIV4;
  #ifdef PANDA
    RCC->PLLCFGR = RCC_PLLCFGR_PLLQ_2 | RCC_PLLCFGR_PLLM_3 |
                   RCC_PLLCFGR_PLLN_6 | RCC_PLLCFGR_PLLN_5 | RCC_PLLCFGR_PLLSRC_HSE;
  #else
    RCC->PLLCFGR = RCC_PLLCFGR_PLLQ_2 | RCC_PLLCFGR_PLLM_3 |
                   RCC_PLLCFGR_PLLN_7 | RCC_PLLCFGR_PLLN_6 | RCC_PLLCFGR_PLLSRC_HSE;
  #endif

  // start PLL
  RCC->CR |= RCC_CR_PLLON;
  while ((RCC->CR & RCC_CR_PLLRDY) == 0);

  // Configure Flash prefetch, Instruction cache, Data cache and wait state
  // *** without this, it breaks ***
  FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;

  // switch to PLL
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

  // *** running on PLL ***
}


void delay(int a) {
  volatile int i;
  for (i=0;i<a;i++);
}

void *memset(void *str, int c, unsigned int n) {
  int i;
  for (i = 0; i < n; i++) {
    *((uint8_t*)str) = c;
    ++str;
  }
  return str;
}

void *memcpy(void *dest, const void *src, unsigned int n) {
  int i;
  // TODO: make not slow
  for (i = 0; i < n; i++) {
    ((uint8_t*)dest)[i] = *(uint8_t*)src;
    ++src;
  }
  return dest;
}

int memcmp(const void * ptr1, const void * ptr2, unsigned int num) {
  int i;
  for (i = 0; i < num; i++) {
    if ( ((uint8_t*)ptr1)[i] != ((uint8_t*)ptr2)[i] ) return -1;
  }
  return 0;
}
