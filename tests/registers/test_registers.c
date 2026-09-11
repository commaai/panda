#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board/stm32h7/inc/stm32h7xx.h"

// Only GPIOA has backing memory. Reading an inactive peripheral would fail.
static GPIO_TypeDef gpio_a;
static I2C_TypeDef i2c_5;
#undef I2C5
#define I2C5 (&i2c_5)
#undef GPIOA
#define GPIOA (&gpio_a)

#include "board/stm32h7/registers.h"

static unsigned int critical_depth;
#define ENTER_CRITICAL() critical_depth++;
#define EXIT_CRITICAL() assert(critical_depth > 0U); critical_depth--;
#define FAULT_REGISTER_DIVERGENT 1U
static unsigned int fault_count;
static unsigned int log_count;

static void print(const char *text) { (void)text; log_count++; }
static void puth(unsigned int value) { (void)value; }
static void fault_occurred(uint32_t fault) {
  assert(fault == FAULT_REGISTER_DIVERGENT);
  fault_count++;
}

#include "board/drivers/registers.h"
#include "board/drivers/gpio.h"

static uint32_t microsecond_timer_get(void) { return 0U; }
static uint32_t get_ts_elapsed(uint32_t now, uint32_t start) { return now - start; }
#include "board/stm32h7/lli2c.h"

static void reset(void) {
  memset(&gpio_a, 0, sizeof(gpio_a));
  // Early initialization must work even before the startup code clears RAM.
  for (unsigned int i = 0U; i < sizeof(tracked_registers) / sizeof(tracked_registers[0]); i++) {
    tracked_registers[i]->state->expected = UINT32_MAX;
    tracked_registers[i]->state->check_mask = UINT32_MAX;
    tracked_registers[i]->state->logged_fault = true;
  }
  init_registers();
  fault_count = 0U;
  log_count = 0U;
  assert(critical_depth == 0U);
}

int main(void) {
  // Every hardware address has exactly one owner.
  const unsigned int count = sizeof(tracked_registers) / sizeof(tracked_registers[0]);
  for (unsigned int i = 0U; i < count; i++) {
    for (unsigned int j = i + 1U; j < count; j++) {
      assert(tracked_registers[i]->address != tracked_registers[j]->address);
      assert(tracked_registers[i]->state != tracked_registers[j]->state);
    }
  }

  reset();
  check_registers();
  assert(fault_count == 0U);

  // Setting and clearing fields preserves unrelated bits and accumulates coverage.
  gpio_a.ODR = 0x80U;
  register_set_bits(&tracked_GPIOA.ODR, 3U);
  register_clear_bits(&tracked_GPIOA.ODR, 1U);
  assert(gpio_a.ODR == 0x82U);
  assert(tracked_GPIOA.ODR.state->expected == 2U);
  assert(tracked_GPIOA.ODR.state->check_mask == 3U);
  check_registers();
  assert(fault_count == 0U);

  // An unrelated GPIO mode update must not accept corrupted hardware as expected.
  reset();
  set_gpio_mode(&tracked_GPIOA, 0U, MODE_OUTPUT);
  gpio_a.MODER ^= 1U;
  set_gpio_mode(&tracked_GPIOA, 1U, MODE_ALTERNATE);
  check_registers();
  assert(fault_count == 1U);
  assert(tracked_GPIOA.MODER.state->expected == 9U);
  unsigned int first_log_count = log_count;
  check_registers();
  assert(fault_count == 2U);
  assert(log_count == first_log_count);

  // A write through the peripheral helper shares state with a direct tracked write.
  set_gpio_mode(&tracked_GPIOA, 0U, MODE_OUTPUT);
  register_set(&tracked_GPIOA.MODER, MODE_INPUT << 2U, 3U << 2U);
  fault_count = 0U;
  check_registers();
  assert(fault_count == 0U);

  reset();
  set_gpio_pullup(&tracked_GPIOA, 0U, PULL_UP);
  gpio_a.PUPDR ^= 1U;
  set_gpio_pullup(&tracked_GPIOA, 1U, PULL_DOWN);
  check_registers();
  assert(fault_count == 1U);

  // Both alternate-function words use field masks, including the last pin.
  for (unsigned int pin = 0U; pin <= 8U; pin += 8U) {
    reset();
    set_gpio_alternate(&tracked_GPIOA, pin, 1U);
    gpio_a.AFR[pin / 8U] ^= 1U;
    set_gpio_alternate(&tracked_GPIOA, pin + 7U, 2U);
    check_registers();
    assert(fault_count == 1U);
    assert(gpio_a.AFR[pin / 8U] == (2U << 28U));
  }

  // I2C transfer direction changes update the same object; command bits aren't checked.
  reset();
  i2c_start(&tracked_I2C5, I2C_CR2_AUTOEND | (2U << I2C_CR2_NBYTES_Pos));
  assert(i2c_5.CR2 == (I2C_CR2_AUTOEND | (2U << I2C_CR2_NBYTES_Pos) | I2C_CR2_START));
  i2c_5.CR2 &= ~I2C_CR2_START;
  check_registers();
  assert(fault_count == 0U);
  i2c_start(&tracked_I2C5, I2C_CR2_RD_WRN | (1U << I2C_CR2_NBYTES_Pos));
  assert(tracked_I2C5.CR2.state->expected == I2C_CR2_RD_WRN);
  check_registers();
  assert(fault_count == 0U);
  i2c_5.CR2 ^= I2C_CR2_RD_WRN;
  check_registers();
  assert(fault_count == 1U);

  // Reinitialization clears coverage and permits reporting a subsequent fault again.
  reset();
  assert(!tracked_GPIOA.AFR1.state->logged_fault);
  check_registers();
  assert(fault_count == 0U);
  set_gpio_mode(&tracked_GPIOA, 0U, MODE_OUTPUT);
  gpio_a.MODER = 0U;
  check_registers();
  assert(fault_count == 1U);
  assert(log_count > 0U);
  assert(critical_depth == 0U);
  return 0;
}
