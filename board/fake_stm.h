// minimal code to fake a panda for tests
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

#define ALLOW_DEBUG

#define ENTER_CRITICAL() do {} while (0);
#define EXIT_CRITICAL() do {} while (0);

void print(const char *a) {
  printf("%s", a);
}

void puth(unsigned int i) {
  printf("%u", i);
}

typedef struct {
  uint32_t CNT;
} TIM_TypeDef;

TIM_TypeDef timer;
TIM_TypeDef *MICROSECOND_TIMER = &timer;
uint32_t microsecond_timer_get(void);

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}

typedef struct {
  uint32_t MODER;
  uint32_t OTYPER;
  uint32_t OSPEEDR;
  uint32_t PUPDR;
  uint32_t IDR;
  uint32_t ODR;
  uint32_t AFR[2];
} GPIO_TypeDef;

GPIO_TypeDef fake_GPIOA;
GPIO_TypeDef fake_GPIOB;
GPIO_TypeDef fake_GPIOC;
GPIO_TypeDef fake_GPIOD;
GPIO_TypeDef fake_GPIOE;
GPIO_TypeDef fake_GPIOF;
GPIO_TypeDef fake_GPIOG;
GPIO_TypeDef fake_GPIOH;

#define GPIOA (&fake_GPIOA)
#define GPIOB (&fake_GPIOB)
#define GPIOC (&fake_GPIOC)
#define GPIOD (&fake_GPIOD)
#define GPIOE (&fake_GPIOE)
#define GPIOF (&fake_GPIOF)
#define GPIOG (&fake_GPIOG)
#define GPIOH (&fake_GPIOH)

void fake_stm_reset_gpio(void) {
  fake_GPIOA = (GPIO_TypeDef){0};
  fake_GPIOB = (GPIO_TypeDef){0};
  fake_GPIOC = (GPIO_TypeDef){0};
  fake_GPIOD = (GPIO_TypeDef){0};
  fake_GPIOE = (GPIO_TypeDef){0};
  fake_GPIOF = (GPIO_TypeDef){0};
  fake_GPIOG = (GPIO_TypeDef){0};
  fake_GPIOH = (GPIO_TypeDef){0};
}
