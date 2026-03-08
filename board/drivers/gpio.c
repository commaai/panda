#include "board/config.h"
#include "board/drivers/gpio.h"
#include "board/drivers/drivers.h"
#include "board/drivers/interrupts.h"
#include "board/drivers/registers.h"

void early_gpio_float(void) {
  RCC->AHB4ENR = RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN | RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN | RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;
  GPIOA->MODER = 0xAB000000U; GPIOB->MODER = 0; GPIOC->MODER = 0; GPIOD->MODER = 0; GPIOE->MODER = 0; GPIOF->MODER = 0; GPIOG->MODER = 0; GPIOH->MODER = 0;
  GPIOA->ODR = 0; GPIOB->ODR = 0; GPIOC->ODR = 0; GPIOD->ODR = 0; GPIOE->ODR = 0; GPIOF->ODR = 0; GPIOG->ODR = 0; GPIOH->ODR = 0;
  GPIOA->PUPDR = 0; GPIOB->PUPDR = 0; GPIOC->PUPDR = 0; GPIOD->PUPDR = 0; GPIOE->PUPDR = 0; GPIOF->PUPDR = 0; GPIOG->PUPDR = 0; GPIOH->PUPDR = 0;
}

void set_gpio_mode(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->MODER;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
  register_set(&(GPIO->MODER), tmp, 0xFFFFFFFFU);
  EXIT_CRITICAL();
}

void set_gpio_output(GPIO_TypeDef *GPIO, unsigned int pin, bool enabled) {
  ENTER_CRITICAL();
  if (enabled) {
    register_set_bits(&(GPIO->ODR), (1UL << pin));
  } else {
    register_clear_bits(&(GPIO->ODR), (1UL << pin));
  }
  set_gpio_mode(GPIO, pin, MODE_OUTPUT);
  EXIT_CRITICAL();
}

void set_gpio_output_type(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int output_type){
  ENTER_CRITICAL();
  if(output_type == OUTPUT_TYPE_OPEN_DRAIN) {
    register_set_bits(&(GPIO->OTYPER), (1UL << pin));
  } else {
    register_clear_bits(&(GPIO->OTYPER), (1U << pin));
  }
  EXIT_CRITICAL();
}

void set_gpio_alternate(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->AFR[pin >> 3U];
  tmp &= ~(0xFU << ((pin & 7U) * 4U));
  tmp |= mode << ((pin & 7U) * 4U);
  register_set(&(GPIO->AFR[pin >> 3]), tmp, 0xFFFFFFFFU);
  set_gpio_mode(GPIO, pin, MODE_ALTERNATE);
  EXIT_CRITICAL();
}

void set_gpio_pullup(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->PUPDR;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
  register_set(&(GPIO->PUPDR), tmp, 0xFFFFFFFFU);
  EXIT_CRITICAL();
}

int get_gpio_input(const GPIO_TypeDef *GPIO, unsigned int pin) {
  return (GPIO->IDR & (1UL << pin)) == (1UL << pin);
}

#ifdef PANDA_JUNGLE
void gpio_set_all_output(gpio_t *pins, uint8_t num_pins, bool enabled) {
  for (uint8_t i = 0; i < num_pins; i++) {
    set_gpio_output(pins[i].bank, pins[i].pin, enabled);
  }
}

void gpio_set_bitmask(gpio_t *pins, uint8_t num_pins, uint32_t bitmask) {
  for (uint8_t i = 0; i < num_pins; i++) {
    set_gpio_output(pins[i].bank, pins[i].pin, (bitmask >> i) & 1U);
  }
}
#endif

bool detect_with_pull(GPIO_TypeDef *GPIO, int pin, int mode) {
  set_gpio_mode(GPIO, pin, MODE_INPUT);
  set_gpio_pullup(GPIO, pin, mode);
  for (volatile int i=0; i<PULL_EFFECTIVE_DELAY; i++);
  bool ret = get_gpio_input(GPIO, pin);
  set_gpio_pullup(GPIO, pin, PULL_NONE);
  return ret;
}
