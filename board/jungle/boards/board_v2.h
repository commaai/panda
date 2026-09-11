#pragma once

#include "board/config.h"

uint8_t harness_orientation = HARNESS_ORIENTATION_NONE;
uint8_t can_mode = CAN_MODE_NORMAL;
uint8_t ignition = 0U;

// ///////////////////////// //
// Jungle board v2 (STM32H7) //
// ///////////////////////// //

#define ADC_CHANNEL(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_810_CYCLES, .oversampling = OVERSAMPLING_1}

gpio_t power_pins[] = {
  {.bank = &tracked_GPIOA, .pin = 0},
  {.bank = &tracked_GPIOA, .pin = 1},
  {.bank = &tracked_GPIOF, .pin = 12},
  {.bank = &tracked_GPIOA, .pin = 5},
  {.bank = &tracked_GPIOC, .pin = 5},
  {.bank = &tracked_GPIOB, .pin = 2},
};

gpio_t sbu1_ignition_pins[] = {
  {.bank = &tracked_GPIOD, .pin = 0},
  {.bank = &tracked_GPIOD, .pin = 5},
  {.bank = &tracked_GPIOD, .pin = 12},
  {.bank = &tracked_GPIOD, .pin = 14},
  {.bank = &tracked_GPIOE, .pin = 5},
  {.bank = &tracked_GPIOE, .pin = 9},
};

gpio_t sbu1_relay_pins[] = {
  {.bank = &tracked_GPIOD, .pin = 1},
  {.bank = &tracked_GPIOD, .pin = 6},
  {.bank = &tracked_GPIOD, .pin = 11},
  {.bank = &tracked_GPIOD, .pin = 15},
  {.bank = &tracked_GPIOE, .pin = 6},
  {.bank = &tracked_GPIOE, .pin = 10},
};

gpio_t sbu2_ignition_pins[] = {
  {.bank = &tracked_GPIOD, .pin = 3},
  {.bank = &tracked_GPIOD, .pin = 8},
  {.bank = &tracked_GPIOD, .pin = 9},
  {.bank = &tracked_GPIOE, .pin = 0},
  {.bank = &tracked_GPIOE, .pin = 7},
  {.bank = &tracked_GPIOE, .pin = 11},
};

gpio_t sbu2_relay_pins[] = {
  {.bank = &tracked_GPIOD, .pin = 4},
  {.bank = &tracked_GPIOD, .pin = 10},
  {.bank = &tracked_GPIOD, .pin = 13},
  {.bank = &tracked_GPIOE, .pin = 1},
  {.bank = &tracked_GPIOE, .pin = 8},
  {.bank = &tracked_GPIOE, .pin = 12},
};

const adc_signal_t sbu1_channels[] = {
  ADC_CHANNEL(ADC3, 12),
  ADC_CHANNEL(ADC3, 2),
  ADC_CHANNEL(ADC3, 4),
  ADC_CHANNEL(ADC3, 6),
  ADC_CHANNEL(ADC3, 8),
  ADC_CHANNEL(ADC3, 10),
};

const adc_signal_t sbu2_channels[] = {
  ADC_CHANNEL(ADC1, 13),
  ADC_CHANNEL(ADC3, 3),
  ADC_CHANNEL(ADC3, 5),
  ADC_CHANNEL(ADC3, 7),
  ADC_CHANNEL(ADC3, 9),
  ADC_CHANNEL(ADC3, 11),
};

void board_v2_set_harness_orientation(uint8_t orientation) {
  switch (orientation) {
    case HARNESS_ORIENTATION_NONE:
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), false);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_1:
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), true);
      gpio_set_bitmask(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), ignition);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), false);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_2:
      gpio_set_bitmask(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), ignition);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), true);
      harness_orientation = orientation;
      break;
    default:
      print("Tried to set an unsupported harness orientation: "); puth(orientation); print("\n");
      break;
  }
}

void board_v2_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  switch (transceiver) {
    case 1U:
      set_gpio_output(&tracked_GPIOG, 11, !enabled);
      break;
    case 2U:
      set_gpio_output(&tracked_GPIOB, 3, !enabled);
      break;
    case 3U:
      set_gpio_output(&tracked_GPIOD, 7, !enabled);
      break;
    case 4U:
      set_gpio_output(&tracked_GPIOB, 4, !enabled);
      break;
    default:
      print("Invalid CAN transceiver ("); puth(transceiver); print("): enabling failed\n");
      break;
  }
}

void board_v2_enable_header_pin(uint8_t pin_num, bool enabled) {
  if (pin_num < 8U) {
    set_gpio_output(&tracked_GPIOG, pin_num, enabled);
  } else {
    print("Invalid pin number ("); puth(pin_num); print("): enabling failed\n");
  }
}

void board_v2_set_can_mode(uint8_t mode) {
  board_v2_enable_can_transceiver(2U, false);
  board_v2_enable_can_transceiver(4U, false);
  switch (mode) {
    case CAN_MODE_NORMAL:
      // B12,B13: disable normal mode
      set_gpio_pullup(&tracked_GPIOB, 12, PULL_NONE);
      set_gpio_mode(&tracked_GPIOB, 12, MODE_ANALOG);

      set_gpio_pullup(&tracked_GPIOB, 13, PULL_NONE);
      set_gpio_mode(&tracked_GPIOB, 13, MODE_ANALOG);

      // B5,B6: FDCAN2 mode
      set_gpio_pullup(&tracked_GPIOB, 5, PULL_NONE);
      set_gpio_alternate(&tracked_GPIOB, 5, GPIO_AF9_FDCAN2);

      set_gpio_pullup(&tracked_GPIOB, 6, PULL_NONE);
      set_gpio_alternate(&tracked_GPIOB, 6, GPIO_AF9_FDCAN2);
      can_mode = CAN_MODE_NORMAL;
      board_v2_enable_can_transceiver(2U, true);
      break;
    case CAN_MODE_OBD_CAN2:
      // B5,B6: disable normal mode
      set_gpio_pullup(&tracked_GPIOB, 5, PULL_NONE);
      set_gpio_mode(&tracked_GPIOB, 5, MODE_ANALOG);

      set_gpio_pullup(&tracked_GPIOB, 6, PULL_NONE);
      set_gpio_mode(&tracked_GPIOB, 6, MODE_ANALOG);
      // B12,B13: FDCAN2 mode
      set_gpio_pullup(&tracked_GPIOB, 12, PULL_NONE);
      set_gpio_alternate(&tracked_GPIOB, 12, GPIO_AF9_FDCAN2);

      set_gpio_pullup(&tracked_GPIOB, 13, PULL_NONE);
      set_gpio_alternate(&tracked_GPIOB, 13, GPIO_AF9_FDCAN2);
      can_mode = CAN_MODE_OBD_CAN2;
      board_v2_enable_can_transceiver(4U, true);
      break;
    default:
      break;
  }
}

uint8_t panda_power_bitmask = 0U;
void board_v2_set_panda_power(bool enable) {
  panda_power = enable;
  gpio_set_all_output(power_pins, sizeof(power_pins) / sizeof(gpio_t), enable);
  if (enable) {
    panda_power_bitmask = 0xFFU;
  } else {
    panda_power_bitmask = 0U;
  }
}

void board_v2_set_panda_individual_power(uint8_t port_num, bool enable) {
  port_num -= 1U;
  if (port_num < 6U) {
    panda_power_bitmask &= ~(1U << port_num);
    panda_power_bitmask |= (enable ? 1U : 0U) << port_num;
  } else {
    print("Invalid port number ("); puth(port_num); print("): enabling failed\n");
  }
  gpio_set_bitmask(power_pins, sizeof(power_pins) / sizeof(gpio_t), (uint32_t)panda_power_bitmask);
}

bool board_v2_get_button(void) {
  return get_gpio_input(&tracked_GPIOG, 15);
}

void board_v2_set_ignition(bool enabled) {
  ignition = enabled ? 0xFFU : 0U;
  board_v2_set_harness_orientation(harness_orientation);
}

void board_v2_set_individual_ignition(uint8_t bitmask) {
  ignition = bitmask;
  board_v2_set_harness_orientation(harness_orientation);
}

float board_v2_get_channel_power(uint8_t channel) {
  float ret = 0.0f;
  if ((channel >= 1U) && (channel <= 6U)) {
    uint16_t readout = adc_get_mV(&(const adc_signal_t) ADC_CHANNEL(ADC1, channel - 1U)); // these are mapped nicely in hardware
    ret = (((float) readout / 33e6) - 0.8e-6) / 52e-6 * 12.0f;
  } else {
    print("Invalid channel ("); puth(channel); print(")\n");
  }
  return ret;
}

uint16_t board_v2_get_sbu_mV(uint8_t channel, uint8_t sbu) {
  uint16_t ret = 0U;
  if ((channel >= 1U) && (channel <= 6U)) {
    switch(sbu){
      case SBU1:
        ret = adc_get_mV(&sbu1_channels[channel - 1U]);
        break;
      case SBU2:
        ret = adc_get_mV(&sbu2_channels[channel - 1U]);
        break;
      default:
        print("Invalid SBU ("); puth(sbu); print(")\n");
        break;
    }
  } else {
    print("Invalid channel ("); puth(channel); print(")\n");
  }
  return ret;
}

void board_v2_init(void) {
  common_init_gpio();

  // Normal CAN mode
  board_v2_set_can_mode(CAN_MODE_NORMAL);

  // Enable CAN transceivers
  for(uint8_t i = 1; i <= 4; i++) {
    board_v2_enable_can_transceiver(i, true);
  }

  // Set to no harness orientation
  board_v2_set_harness_orientation(HARNESS_ORIENTATION_NONE);

  // Enable panda power by default
  board_v2_set_panda_power(true);

  // Current monitor channels
  adc_init(ADC1);
  register_set_bits(&tracked_SYSCFG_PMCR, SYSCFG_PMCR_PA0SO | SYSCFG_PMCR_PA1SO); // open up analog switches for PA0_C and PA1_C
  set_gpio_mode(&tracked_GPIOF, 11, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOA, 6, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOC, 4, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOB, 1, MODE_ANALOG);

  // SBU channels
  adc_init(ADC3);
  set_gpio_mode(&tracked_GPIOC, 2, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOC, 3, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 9, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 7, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 5, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 3, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 10, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 8, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 6, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOF, 4, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOC, 0, MODE_ANALOG);
  set_gpio_mode(&tracked_GPIOC, 1, MODE_ANALOG);

  // Header pins
  set_gpio_mode(&tracked_GPIOG, 0, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 1, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 2, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 3, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 4, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 5, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 6, MODE_OUTPUT);
  set_gpio_mode(&tracked_GPIOG, 7, MODE_OUTPUT);
}

board board_v2 = {
  .init = &board_v2_init,
  .led_GPIO = {&tracked_GPIOE, &tracked_GPIOE, &tracked_GPIOE},
  .led_pin = {4, 3, 2},
  .get_button = &board_v2_get_button,
  .set_panda_power = &board_v2_set_panda_power,
  .set_panda_individual_power = &board_v2_set_panda_individual_power,
  .set_ignition = &board_v2_set_ignition,
  .set_individual_ignition = &board_v2_set_individual_ignition,
  .set_harness_orientation = &board_v2_set_harness_orientation,
  .set_can_mode = &board_v2_set_can_mode,
  .enable_can_transceiver = &board_v2_enable_can_transceiver,
  .enable_header_pin = &board_v2_enable_header_pin,
  .get_channel_power = &board_v2_get_channel_power,
  .get_sbu_mV = &board_v2_get_sbu_mV,
};
