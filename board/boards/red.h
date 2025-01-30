#pragma once

#include "board_declarations.h"

// ///////////////////////////// //
// Red Panda (STM32H7) + Harness //
// ///////////////////////////// //

static void red_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  switch (transceiver) {
    case 1U:
      set_gpio_output(GPIOG, 11, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOB, 3, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOD, 7, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 4, !enabled);
      break;
    default:
      break;
  }
}

static void red_enable_can_transceivers(bool enabled) {
  uint8_t main_bus = (harness.status == HARNESS_STATUS_FLIPPED) ? 3U : 1U;
  for (uint8_t i=1U; i<=4U; i++) {
    // Leave main CAN always on for CAN-based ignition detection
    if (i == main_bus) {
      red_enable_can_transceiver(i, true);
    } else {
      red_enable_can_transceiver(i, enabled);
    }
  }
}

static void red_set_led(uint8_t color, bool enabled) {
  switch (color) {
    case LED_RED:
      set_gpio_output(GPIOE, 4, !enabled);
      break;
     case LED_GREEN:
      set_gpio_output(GPIOE, 3, !enabled);
      break;
    case LED_BLUE:
      set_gpio_output(GPIOE, 2, !enabled);
      break;
    default:
      break;
  }
}

static bool red_check_ignition(void) {
  // ignition is checked through harness
  return harness_check_ignition();
}

static uint32_t red_read_voltage_mV(void){
  return adc_get_mV(2) * 11U; // TODO: is this correct?
}

static void red_init(void) {
  common_init_gpio();

  // G11,B3,D7,B4: transceiver enable
  set_gpio_pullup(GPIOG, 11, PULL_NONE);
  set_gpio_mode(GPIOG, 11, MODE_OUTPUT);

  set_gpio_pullup(GPIOB, 3, PULL_NONE);
  set_gpio_mode(GPIOB, 3, MODE_OUTPUT);

  set_gpio_pullup(GPIOD, 7, PULL_NONE);
  set_gpio_mode(GPIOD, 7, MODE_OUTPUT);

  set_gpio_pullup(GPIOB, 4, PULL_NONE);
  set_gpio_mode(GPIOB, 4, MODE_OUTPUT);

  //B1: 5VOUT_S
  set_gpio_pullup(GPIOB, 1, PULL_NONE);
  set_gpio_mode(GPIOB, 1, MODE_ANALOG);

  // B14: usb load switch, enabled by pull resistor on board, obsolete for red panda
  set_gpio_output_type(GPIOB, 14, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_pullup(GPIOB, 14, PULL_UP);
  set_gpio_mode(GPIOB, 14, MODE_OUTPUT);
  set_gpio_output(GPIOB, 14, 1);
}

static harness_configuration red_harness_config = {
  .has_harness = true,
  .GPIO_SBU1 = GPIOC,
  .GPIO_SBU2 = GPIOA,
  .GPIO_relay_SBU1 = GPIOC,
  .GPIO_relay_SBU2 = GPIOC,
  .pin_SBU1 = 4,
  .pin_SBU2 = 1,
  .pin_relay_SBU1 = 10,
  .pin_relay_SBU2 = 11,
  .adc_channel_SBU1 = 4, //ADC12_INP4
  .adc_channel_SBU2 = 17, //ADC1_INP17
  .GPIO_CAN2_RX_NORMAL = GPIOB,
  .GPIO_CAN2_TX_NORMAL = GPIOB,
  .GPIO_CAN2_RX_FLIPPED = GPIOB,
  .GPIO_CAN2_TX_FLIPPED = GPIOB,
  .pin_CAN2_RX_NORMAL = 12,
  .pin_CAN2_TX_NORMAL = 13,
  .pin_CAN2_RX_FLIPPED = 5,
  .pin_CAN2_TX_FLIPPED = 6
};

board board_red = {
  .set_bootkick = unused_set_bootkick,
  .harness_config = &red_harness_config,
  .has_obd = true,
  .has_spi = false,
  .has_canfd = true,
  .fan_max_rpm = 0U,
  .fan_max_pwm = 100U,
  .avdd_mV = 3300U,
  .fan_stall_recovery = false,
  .fan_enable_cooldown_time = 0U,
  .init = red_init,
  .init_bootloader = unused_init_bootloader,
  .enable_can_transceiver = red_enable_can_transceiver,
  .enable_can_transceivers = red_enable_can_transceivers,
  .set_led = red_set_led,
  .check_ignition = red_check_ignition,
  .read_voltage_mV = red_read_voltage_mV,
  .read_current_mA = unused_read_current,
  .set_fan_enabled = unused_set_fan_enabled,
  .set_ir_power = unused_set_ir_power,
  .set_siren = unused_set_siren,
  .read_som_gpio = unused_read_som_gpio,
  .set_amp_enabled = unused_set_amp_enabled
};
