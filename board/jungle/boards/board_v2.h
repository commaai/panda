#pragma once

// ///////////////////////// //
// Jungle board v2 (STM32H7) //
// ///////////////////////// //

#include "board/drivers/registers.h"
#include "board/board_struct.h"
#include "board/drivers/gpio.h"

#ifndef PANDA_JUNGLE
#error This should only be used on Panda Body!
#endif

#define ADC_CHANNEL(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_810_CYCLES, .oversampling = OVERSAMPLING_1}

gpio_t power_pins[] = {
  {.bank = GPIOA, .pin = 0},
  {.bank = GPIOA, .pin = 1},
  {.bank = GPIOF, .pin = 12},
  {.bank = GPIOA, .pin = 5},
  {.bank = GPIOC, .pin = 5},
  {.bank = GPIOB, .pin = 2},
};

gpio_t sbu1_ignition_pins[] = {
  {.bank = GPIOD, .pin = 0},
  {.bank = GPIOD, .pin = 5},
  {.bank = GPIOD, .pin = 12},
  {.bank = GPIOD, .pin = 14},
  {.bank = GPIOE, .pin = 5},
  {.bank = GPIOE, .pin = 9},
};

gpio_t sbu1_relay_pins[] = {
  {.bank = GPIOD, .pin = 1},
  {.bank = GPIOD, .pin = 6},
  {.bank = GPIOD, .pin = 11},
  {.bank = GPIOD, .pin = 15},
  {.bank = GPIOE, .pin = 6},
  {.bank = GPIOE, .pin = 10},
};

gpio_t sbu2_ignition_pins[] = {
  {.bank = GPIOD, .pin = 3},
  {.bank = GPIOD, .pin = 8},
  {.bank = GPIOD, .pin = 9},
  {.bank = GPIOE, .pin = 0},
  {.bank = GPIOE, .pin = 7},
  {.bank = GPIOE, .pin = 11},
};

gpio_t sbu2_relay_pins[] = {
  {.bank = GPIOD, .pin = 4},
  {.bank = GPIOD, .pin = 10},
  {.bank = GPIOD, .pin = 13},
  {.bank = GPIOE, .pin = 1},
  {.bank = GPIOE, .pin = 8},
  {.bank = GPIOE, .pin = 12},
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
