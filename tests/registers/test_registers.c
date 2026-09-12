#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "board/stm32h7/inc/stm32h7xx.h"

// Only GPIOA has backing memory. The checker must not read inactive peripherals.
static GPIO_TypeDef gpio_a;
#undef GPIOA
#define GPIOA (&gpio_a)
#include "board/stm32h7/registers.h"

static unsigned int critical_depth;
#define ENTER_CRITICAL() critical_depth++;
#define EXIT_CRITICAL() assert(critical_depth > 0U); critical_depth--;
#define FAULT_REGISTER_DIVERGENT 1U
#define NUM_INTERRUPTS 163U
static unsigned int fault_count;
static unsigned int log_count;
static volatile uint32_t unknown_register;

void print(const char *text) { (void)text; log_count++; }
void puth(unsigned int value) { (void)value; }
void fault_occurred(uint32_t fault) {
  assert(fault == FAULT_REGISTER_DIVERGENT);
  fault_count++;
}
void assert_fatal(bool condition, const char *message) {
  if (!condition) {
    assert(strcmp(message, "Register missing from monitoring inventory\n") == 0);
    assert(unknown_register == 0U);
    exit(23);
  }
}

#include "board/drivers/registers.h"
#include "board/drivers/gpio.h"

static void reset(void) {
  memset(&gpio_a, 0, sizeof(gpio_a));
  // Simulate RAM before startup has cleared it.
  memset(register_map, 0xFF, sizeof(register_map));
  init_registers();
  fault_count = 0U;
  log_count = 0U;
  assert(critical_depth == 0U);
}

int main(int argc, char **argv) {
  (void)argv;
  reset();
  if (argc > 1) {
    register_set(&unknown_register, 1U, 1U);
    return 1; // Unknown addresses must fail before writing hardware.
  }

  for (unsigned int i = 0U; i < REGISTER_COUNT; i++) {
    for (unsigned int j = i + 1U; j < REGISTER_COUNT; j++) {
      assert(register_addresses[i] != register_addresses[j]);
    }
  }
  check_registers();
  assert(fault_count == 0U);

  gpio_a.ODR = 0x80U;
  register_set_bits(&GPIOA->ODR, 3U);
  register_clear_bits(&GPIOA->ODR, 1U);
  assert(gpio_a.ODR == 0x82U);
  check_registers();
  assert(fault_count == 0U);

  // Updating another field must not accept existing corruption as expected state.
  reset();
  set_gpio_mode(GPIOA, 0U, MODE_OUTPUT);
  gpio_a.MODER ^= 1U;
  set_gpio_mode(GPIOA, 1U, MODE_ALTERNATE);
  check_registers();
  assert(fault_count == 1U);
  unsigned int first_log_count = log_count;
  check_registers();
  assert(fault_count == 2U);
  assert(log_count == first_log_count);

  // Different callers share the same expected value for this hardware address.
  set_gpio_mode(GPIOA, 0U, MODE_OUTPUT);
  register_set(&GPIOA->MODER, MODE_INPUT << 2U, 3U << 2U);
  fault_count = 0U;
  check_registers();
  assert(fault_count == 0U);

  reset();
  set_gpio_pullup(GPIOA, 0U, PULL_UP);
  gpio_a.PUPDR ^= 1U;
  set_gpio_pullup(GPIOA, 1U, PULL_DOWN);
  check_registers();
  assert(fault_count == 1U);

  for (unsigned int pin = 0U; pin <= 8U; pin += 8U) {
    reset();
    set_gpio_alternate(GPIOA, pin, 1U);
    gpio_a.AFR[pin / 8U] ^= 1U;
    set_gpio_alternate(GPIOA, pin + 7U, 2U);
    check_registers();
    assert(fault_count == 1U);
    assert(gpio_a.AFR[pin / 8U] == (2U << 28U));
  }
  assert(critical_depth == 0U);
  return 0;
}
