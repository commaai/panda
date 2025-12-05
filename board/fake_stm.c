#include "fake_stm.h"

#include <stdio.h>

void print(const char *a) {
  printf("%s", a);
}

void puth(unsigned int i) {
  printf("%u", i);
}

TIM_TypeDef timer;
TIM_TypeDef *MICROSECOND_TIMER = &timer;

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}


// NOCHECKIN
void put_char(char c) {};