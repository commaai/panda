#include <stdio.h>
#include "board/fake_stm.h"

TIM_TypeDef timer;
TIM_TypeDef *MICROSECOND_TIMER = &timer;

void print(const char *a) {
  printf("%s", a);
}

void puth(unsigned int i) {
  printf("%u", i);
}

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}
