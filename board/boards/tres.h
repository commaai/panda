#pragma once

#include "board_declarations.h"

// ///////////////////////////
// Tres (STM32H7) + Harness //
// ///////////////////////////

static bool tres_ir_enabled;
static bool tres_fan_enabled;
static void tres_update_fan_ir_power(void) {
  set_gpio_output(GPIOD, 3, tres_ir_enabled || tres_fan_enabled);
}

static void tres_set_ir_power(uint8_t percentage){
  tres_ir_enabled = (percentage > 0U);
  tres_update_fan_ir_power();
  pwm_set(TIM3, 4, percentage);
}

static void tres_set_bootkick(BootState state) {
  set_gpio_output(GPIOA, 0, state != BOOT_BOOTKICK);
  set_gpio_output(GPIOC, 12, state != BOOT_RESET);
}

static void tres_set_fan_enabled(bool enabled) {
  // NOTE: fan controller reset doesn't work on a tres if IR is enabled
  tres_fan_enabled = enabled;
  tres_update_fan_ir_power();
}

static void tres_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  switch (transceiver) {
    case 1U:
      set_gpio_output(GPIOG, 11, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOB, 10, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOD, 7, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 11, !enabled);
      break;
    default:
      break;
  }
}

static void tres_enable_can_transceivers(bool enabled) {
  uint8_t main_bus = (harness.status == HARNESS_STATUS_FLIPPED) ? 3U : 1U;
  for (uint8_t i=1U; i<=4U; i++) {
    // Leave main CAN always on for CAN-based ignition detection
    if (i == main_bus) {
      tres_enable_can_transceiver(i, true);
    } else {
      tres_enable_can_transceiver(i, enabled);
    }
  }
}

static void tres_set_can_mode(uint8_t mode) {
  current_board->enable_can_transceiver(2U, false);
  current_board->enable_can_transceiver(4U, false);
  switch (mode) {
    case CAN_MODE_NORMAL:
    case CAN_MODE_OBD_CAN2:
      if ((bool)(mode == CAN_MODE_NORMAL) != (bool)(harness.status == HARNESS_STATUS_FLIPPED)) {
        // B12,B13: disable normal mode
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_mode(GPIOB, 12, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_mode(GPIOB, 13, MODE_ANALOG);

        // B5,B6: FDCAN2 mode
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
        current_board->enable_can_transceiver(2U, true);
      } else {
        // B5,B6: disable normal mode
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_mode(GPIOB, 5, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_mode(GPIOB, 6, MODE_ANALOG);
        // B12,B13: FDCAN2 mode
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_alternate(GPIOB, 12, GPIO_AF9_FDCAN2);

        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_alternate(GPIOB, 13, GPIO_AF9_FDCAN2);
        current_board->enable_can_transceiver(4U, true);
      }
      break;
    default:
      break;
  }
}

static bool tres_read_som_gpio (void) {
  return (get_gpio_input(GPIOC, 2) != 0);
}

static void tres_init(void) {
  // Enable USB 3.3V LDO for USB block
  register_set_bits(&(PWR->CR3), PWR_CR3_USBREGEN);
  register_set_bits(&(PWR->CR3), PWR_CR3_USB33DEN);
  while ((PWR->CR3 & PWR_CR3_USB33RDY) == 0U);

  common_init_gpio();

  // A8, A3: OBD_SBU1_RELAY, OBD_SBU2_RELAY
  set_gpio_output_type(GPIOA, 8, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_pullup(GPIOA, 8, PULL_NONE);
  set_gpio_output(GPIOA, 8, 1);
  set_gpio_mode(GPIOA, 8, MODE_OUTPUT);

  set_gpio_output_type(GPIOA, 3, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_pullup(GPIOA, 3, PULL_NONE);
  set_gpio_output(GPIOA, 3, 1);
  set_gpio_mode(GPIOA, 3, MODE_OUTPUT);

  // Initialize harness
  harness_init();

  // C2: SOM GPIO used as input (fan control at boot)
  set_gpio_mode(GPIOC, 2, MODE_INPUT);
  set_gpio_pullup(GPIOC, 2, PULL_DOWN);

  // SOM bootkick + reset lines
  // WARNING: make sure output state is set before configuring as output
  tres_set_bootkick(BOOT_BOOTKICK);
  set_gpio_mode(GPIOC, 12, MODE_OUTPUT);

  // SOM debugging UART
  gpio_uart7_init();
  uart_init(&uart_ring_som_debug, 115200);

  // SPI init
  gpio_spi_init();

  // fan setup
  set_gpio_alternate(GPIOC, 8, GPIO_AF2_TIM3);

  // Initialize IR PWM and set to 0%
  set_gpio_alternate(GPIOC, 9, GPIO_AF2_TIM3);
  pwm_init(TIM3, 4);
  tres_set_ir_power(0U);

  // Fake siren
  set_gpio_alternate(GPIOC, 10, GPIO_AF4_I2C5);
  set_gpio_alternate(GPIOC, 11, GPIO_AF4_I2C5);
  register_set_bits(&(GPIOC->OTYPER), GPIO_OTYPER_OT10 | GPIO_OTYPER_OT11); // open drain

  // Clock source
  clock_source_init();
}

static harness_configuration tres_harness_config = {
  .has_harness = true,
  .GPIO_SBU1 = GPIOC,
  .GPIO_SBU2 = GPIOA,
  .GPIO_relay_SBU1 = GPIOA,
  .GPIO_relay_SBU2 = GPIOA,
  .pin_SBU1 = 4,
  .pin_SBU2 = 1,
  .pin_relay_SBU1 = 8,
  .pin_relay_SBU2 = 3,
  .adc_channel_SBU1 = 4, // ADC12_INP4
  .adc_channel_SBU2 = 17 // ADC1_INP17
};

board board_tres = {
  .harness_config = &tres_harness_config,
  .has_obd = true,
  .has_spi = true,
  .has_canfd = true,
  .fan_max_rpm = 6600U,
  .fan_max_pwm = 100U,
  .avdd_mV = 1800U,
  .fan_stall_recovery = false,
  .fan_enable_cooldown_time = 3U,
  .init = tres_init,
  .init_bootloader = unused_init_bootloader,
  .enable_can_transceiver = tres_enable_can_transceiver,
  .enable_can_transceivers = tres_enable_can_transceivers,
  .set_led = red_set_led,
  .set_can_mode = tres_set_can_mode,
  .check_ignition = red_check_ignition,
  .read_voltage_mV = red_read_voltage_mV,
  .read_current_mA = unused_read_current,
  .set_fan_enabled = tres_set_fan_enabled,
  .set_ir_power = tres_set_ir_power,
  .set_siren = fake_siren_set,
  .set_bootkick = tres_set_bootkick,
  .read_som_gpio = tres_read_som_gpio,
  .set_amp_enabled = unused_set_amp_enabled
};
