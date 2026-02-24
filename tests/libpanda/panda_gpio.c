// Test harness for verifying GPIO initialization of panda boards.
// Compiles against fake_stm_gpio.h which provides simulated GPIO registers.
#include "fake_stm_gpio.h"

// ==================== Fault tracking ====================
#define FAULT_STATUS_NONE 0U
#define FAULT_STATUS_TEMPORARY 1U
#define FAULT_STATUS_PERMANENT 2U
#define PERMANENT_FAULTS 0U
#define FAULT_REGISTER_DIVERGENT (1UL << 18)
#define FAULT_SIREN_MALFUNCTION  (1UL << 25)

uint8_t fault_status = FAULT_STATUS_NONE;
uint32_t faults = 0U;

void fault_occurred(uint32_t fault) { faults |= fault; }
void fault_recovered(uint32_t fault) { faults &= ~fault; }

// ==================== Register map (inlined) ====================

typedef struct reg {
  volatile uint32_t *address;
  uint32_t value;
  uint32_t check_mask;
  bool logged_fault;
} reg;

#define CHECK_COLLISION(hash, addr) (((uint32_t)(uintptr_t)register_map[hash].address != 0U) && (register_map[hash].address != (addr)))

static reg register_map[REGISTER_MAP_SIZE];

static uint16_t hash_addr(uint32_t input){
  return (((input >> 16U) ^ ((((input + 1U) & 0xFFFFU) * HASHING_PRIME) & 0xFFFFU)) & REGISTER_MAP_SIZE);
}

void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask){
  (*addr) = ((*addr) & (~mask)) | (val & mask);
  uint16_t hash = hash_addr((uint32_t)(uintptr_t) addr);
  uint16_t tries = REGISTER_MAP_SIZE;
  while(CHECK_COLLISION(hash, addr) && (tries > 0U)) { hash = hash_addr((uint32_t) hash); tries--;}
  if (tries != 0U){
    register_map[hash].address = addr;
    register_map[hash].value = (register_map[hash].value & (~mask)) | (val & mask);
    register_map[hash].check_mask |= mask;
  }
}

void register_set_bits(volatile uint32_t *addr, uint32_t val) {
  register_set(addr, val, val);
}

void register_clear_bits(volatile uint32_t *addr, uint32_t val) {
  register_set(addr, (~val), val);
}

void init_registers(void) {
  for(uint16_t i=0U; i<REGISTER_MAP_SIZE; i++){
    register_map[i].address = (volatile uint32_t *) 0U;
    register_map[i].check_mask = 0U;
  }
}

// ==================== GPIO driver ====================
#include "drivers/gpio.h"

// ==================== PWM driver ====================
#include "drivers/pwm.h"

// ==================== Harness / Board declarations ====================

#define HARNESS_STATUS_NC 0U
#define HARNESS_STATUS_NORMAL 1U
#define HARNESS_STATUS_FLIPPED 2U

struct harness_t {
  uint8_t status;
  uint16_t sbu1_voltage_mV;
  uint16_t sbu2_voltage_mV;
  bool relay_driven;
  bool sbu_adc_lock;
};
struct harness_t harness;

typedef struct harness_configuration {
  GPIO_TypeDef * const GPIO_SBU1;
  GPIO_TypeDef * const GPIO_SBU2;
  GPIO_TypeDef * const GPIO_relay_SBU1;
  GPIO_TypeDef * const GPIO_relay_SBU2;
  const uint8_t pin_SBU1;
  const uint8_t pin_SBU2;
  const uint8_t pin_relay_SBU1;
  const uint8_t pin_relay_SBU2;
  const adc_signal_t adc_signal_SBU1;
  const adc_signal_t adc_signal_SBU2;
} harness_configuration;

typedef struct board board;

#include "boards/board_declarations.h"

uint8_t hw_type = 0;
board *current_board;
uint32_t uptime_cnt = 0;
uint32_t heartbeat_counter = 0;
bool heartbeat_lost = false;
bool heartbeat_disabled = false;
bool siren_enabled = false;

// ==================== Inlined peripherals (from stm32h7/peripherals.h) ====================

static void gpio_usb_init(void) {
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG1_FS);
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG1_FS);
  GPIOA->OSPEEDR = GPIO_OSPEEDR_OSPEED11 | GPIO_OSPEEDR_OSPEED12;
}

void gpio_spi_init(void) {
  set_gpio_alternate(GPIOE, 11, GPIO_AF5_SPI4);
  set_gpio_alternate(GPIOE, 12, GPIO_AF5_SPI4);
  set_gpio_alternate(GPIOE, 13, GPIO_AF5_SPI4);
  set_gpio_alternate(GPIOE, 14, GPIO_AF5_SPI4);
  register_set_bits(&(GPIOE->OSPEEDR), GPIO_OSPEEDR_OSPEED11 | GPIO_OSPEEDR_OSPEED12 | GPIO_OSPEEDR_OSPEED13 | GPIO_OSPEEDR_OSPEED14);
}

void gpio_uart7_init(void) {
  set_gpio_alternate(GPIOE, 7, GPIO_AF7_UART7);
  set_gpio_alternate(GPIOE, 8, GPIO_AF7_UART7);
}

void common_init_gpio(void) {
  set_gpio_pullup(GPIOF, 11, PULL_NONE);
  set_gpio_mode(GPIOF, 11, MODE_ANALOG);

  gpio_usb_init();

  // B8,B9: FDCAN1
  set_gpio_pullup(GPIOB, 8, PULL_NONE);
  set_gpio_alternate(GPIOB, 8, GPIO_AF9_FDCAN1);

  set_gpio_pullup(GPIOB, 9, PULL_NONE);
  set_gpio_alternate(GPIOB, 9, GPIO_AF9_FDCAN1);

  // B5,B6 (mplex to B12,B13): FDCAN2
  set_gpio_pullup(GPIOB, 12, PULL_NONE);
  set_gpio_pullup(GPIOB, 13, PULL_NONE);

  set_gpio_pullup(GPIOB, 5, PULL_NONE);
  set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

  set_gpio_pullup(GPIOB, 6, PULL_NONE);
  set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);

  // G9,G10: FDCAN3
  set_gpio_pullup(GPIOG, 9, PULL_NONE);
  set_gpio_alternate(GPIOG, 9, GPIO_AF2_FDCAN3);

  set_gpio_pullup(GPIOG, 10, PULL_NONE);
  set_gpio_alternate(GPIOG, 10, GPIO_AF2_FDCAN3);
}

// ==================== Stubs for functions called by board init ====================

// clock_source_init stub (just configures GPIO pins for clock output)
void clock_source_init(bool enable_channel1) {
  // Set timer registers as the real function does
  register_set(&(TIM1->PSC), ((APB2_TIMER_FREQ*100U)-1U), 0xFFFFU);
  register_set(&(TIM1->ARR), ((50U*10U) - 1U), 0xFFFFU);
  register_set(&(TIM1->CCMR1), 0U, 0xFFFFU);
  register_set(&(TIM1->CCER), TIM_CCER_CC1E, 0xFFFFU);
  register_set(&(TIM1->CCR1), (2U*10U), 0xFFFFU);
  register_set(&(TIM1->CCR2), (2U*10U), 0xFFFFU);
  register_set(&(TIM1->CCR3), (2U*10U), 0xFFFFU);
  register_set_bits(&(TIM1->DIER), TIM_DIER_UIE | TIM_DIER_CC1IE);
  register_set(&(TIM1->CR1), TIM_CR1_CEN, 0x3FU);

  NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);
  NVIC_DisableIRQ(TIM1_CC_IRQn);

  // GPIO pins for clock output
  if (enable_channel1) {
    set_gpio_alternate(GPIOA, 8, GPIO_AF1_TIM1);
  }
  set_gpio_alternate(GPIOB, 14, GPIO_AF1_TIM1);
  set_gpio_alternate(GPIOB, 15, GPIO_AF1_TIM1);

  register_set(&(TIM1->CCMR1), (0b110UL << TIM_CCMR1_OC1M_Pos) | (0b110UL << TIM_CCMR1_OC2M_Pos), 0xFFFFU);
  register_set(&(TIM1->CCMR2), (0b110UL << TIM_CCMR2_OC3M_Pos), 0xFFFFU);
  register_set(&(TIM1->BDTR), TIM_BDTR_MOE, 0xFFFFU);
  register_set_bits(&(TIM1->CCER), TIM_CCER_CC2NE | TIM_CCER_CC3NE);
}

// fake_i2c_siren_set and fake_siren_set stubs
void fake_i2c_siren_set(bool enabled) { (void)enabled; }
void fake_siren_set(bool enabled) { (void)enabled; }

// ==================== Unused board functions ====================
#include "boards/unused_funcs.h"

// ==================== Board init code ====================
#include "boards/red.h"
#include "boards/tres.h"
#include "boards/cuatro.h"

// ==================== Exported test functions ====================

void test_reset(void) {
  reset_fake_gpio();
  init_registers();
  harness.status = HARNESS_STATUS_NC;
  harness.relay_driven = false;
  harness.sbu_adc_lock = false;
}

void test_init_red(void) {
  test_reset();
  hw_type = HW_TYPE_RED_PANDA;
  current_board = &board_red;
  red_init();
}

void test_init_tres(void) {
  test_reset();
  hw_type = HW_TYPE_TRES;
  current_board = &board_tres;
  tres_init();
}

void test_init_cuatro(void) {
  test_reset();
  hw_type = HW_TYPE_CUATRO;
  current_board = &board_cuatro;
  cuatro_init();
}

// ==================== GPIO state query functions ====================

uint32_t query_pin_mode(uint8_t bank, uint8_t pin) {
  GPIO_TypeDef *gpios[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
  if (bank >= 8 || pin >= 16) return 0xFF;
  return (gpios[bank]->MODER >> (pin * 2U)) & 3U;
}

uint32_t query_pin_af(uint8_t bank, uint8_t pin) {
  GPIO_TypeDef *gpios[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
  if (bank >= 8 || pin >= 16) return 0xFF;
  return (gpios[bank]->AFR[pin >> 3U] >> ((pin & 7U) * 4U)) & 0xFU;
}

uint32_t query_pin_otype(uint8_t bank, uint8_t pin) {
  GPIO_TypeDef *gpios[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
  if (bank >= 8 || pin >= 16) return 0xFF;
  return (gpios[bank]->OTYPER >> pin) & 1U;
}

uint32_t query_pin_pupd(uint8_t bank, uint8_t pin) {
  GPIO_TypeDef *gpios[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
  if (bank >= 8 || pin >= 16) return 0xFF;
  return (gpios[bank]->PUPDR >> (pin * 2U)) & 3U;
}

uint32_t query_pin_odr(uint8_t bank, uint8_t pin) {
  GPIO_TypeDef *gpios[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
  if (bank >= 8 || pin >= 16) return 0xFF;
  return (gpios[bank]->ODR >> pin) & 1U;
}

uint32_t query_gpio_ospeedr(uint8_t bank) {
  GPIO_TypeDef *gpios[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH};
  if (bank >= 8) return 0;
  return gpios[bank]->OSPEEDR;
}
