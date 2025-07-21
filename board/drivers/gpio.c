#include "gpio.h"

#ifndef BOOTSTUB
// Forward declarations for register functions
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t val);
void register_clear_bits(volatile uint32_t *addr, uint32_t val);
#define ENTER_CRITICAL() __disable_irq()
#define EXIT_CRITICAL() __enable_irq()
#else
#include "board/config.h"
#endif

void set_gpio_mode(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->MODER;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
#ifdef BOOTSTUB
  GPIO->MODER = tmp;
#else
  register_set(&(GPIO->MODER), tmp, 0xFFFFFFFFU);
#endif
  EXIT_CRITICAL();
}

void set_gpio_output(GPIO_TypeDef *GPIO, unsigned int pin, bool enabled) {
  ENTER_CRITICAL();
  if (enabled) {
#ifdef BOOTSTUB
    GPIO->ODR |= (1UL << pin);
#else
    register_set_bits(&(GPIO->ODR), (1UL << pin));
#endif
  } else {
#ifdef BOOTSTUB
    GPIO->ODR &= ~(1UL << pin);
#else
    register_clear_bits(&(GPIO->ODR), (1UL << pin));
#endif
  }
  set_gpio_mode(GPIO, pin, MODE_OUTPUT);
  EXIT_CRITICAL();
}

void set_gpio_output_type(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int output_type){
  ENTER_CRITICAL();
  if(output_type == OUTPUT_TYPE_OPEN_DRAIN) {
#ifdef BOOTSTUB
    GPIO->OTYPER |= (1UL << pin);
#else
    register_set_bits(&(GPIO->OTYPER), (1UL << pin));
#endif
  } else {
#ifdef BOOTSTUB
    GPIO->OTYPER &= ~(1U << pin);
#else
    register_clear_bits(&(GPIO->OTYPER), (1U << pin));
#endif
  }
  EXIT_CRITICAL();
}

void set_gpio_alternate(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->AFR[pin >> 3U];
  tmp &= ~(0xFU << ((pin & 7U) * 4U));
  tmp |= mode << ((pin & 7U) * 4U);
#ifdef BOOTSTUB
  GPIO->AFR[pin >> 3] = tmp;
#else
  register_set(&(GPIO->AFR[pin >> 3]), tmp, 0xFFFFFFFFU);
#endif
  set_gpio_mode(GPIO, pin, MODE_ALTERNATE);
  EXIT_CRITICAL();
}

void set_gpio_pullup(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->PUPDR;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
#ifdef BOOTSTUB
  GPIO->PUPDR = tmp;
#else
  register_set(&(GPIO->PUPDR), tmp, 0xFFFFFFFFU);
#endif
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

// Detection with internal pullup
bool detect_with_pull(GPIO_TypeDef *GPIO, int pin, int mode) {
  set_gpio_mode(GPIO, pin, MODE_INPUT);
  set_gpio_pullup(GPIO, pin, mode);
  for (volatile int i=0; i<PULL_EFFECTIVE_DELAY; i++);
  bool ret = get_gpio_input(GPIO, pin);
  set_gpio_pullup(GPIO, pin, PULL_NONE);
  return ret;
}