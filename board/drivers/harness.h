#pragma once

#include "board/drivers/drivers.h"

struct harness_t harness;

#ifdef ALLOW_DEBUG
uint16_t harness_adc_check_offset_mV = 0U;
#endif

// The ignition relay is only used for testing purposes
void set_intercept_relay(bool intercept, bool ignition_relay) {
  bool drive_relay = intercept;
  if (harness.status == HARNESS_STATUS_NC) {
    // no harness, no relay to drive
    drive_relay = false;
  }

  if (drive_relay || ignition_relay) {
    harness.relay_driven = true;
  }

  // wait until we're not reading the analog voltages anymore
  while (harness.sbu_adc_lock) {}

  if (harness.status == HARNESS_STATUS_NORMAL) {
    set_gpio_output(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, !ignition_relay);
    set_gpio_output(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, !drive_relay);
  } else {
    set_gpio_output(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, !drive_relay);
    set_gpio_output(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, !ignition_relay);
  }

  if (!(drive_relay || ignition_relay)) {
    harness.relay_driven = false;
  }
}

#ifdef ALLOW_DEBUG
bool relay_readback_inject = false;
#endif

static bool relay_pin_mismatch(const GPIO_TypeDef *GPIO, unsigned int pin) {
  bool is_output = ((GPIO->MODER >> (pin * 2U)) & 3U) == (uint32_t)MODE_OUTPUT;
  bool driven = (GPIO->ODR & (1UL << pin)) != 0U;
  bool read = get_gpio_input(GPIO, pin) != 0;
  #ifdef ALLOW_DEBUG
  read = read != relay_readback_inject;
  #endif
  return is_output && (driven != read);
}

// GPIO_SM_2: the relay pins are open drain with a pull-up on the FET gate, so the pin reads back what is driven.
// called at 8Hz, two mismatches in a row are a fault, which leaves time for the gate to settle after a change
void harness_check_relay_readback(void) {
  static uint32_t mismatches = 0U;
  const harness_configuration *cfg = current_board->harness_config;
  bool mismatch = relay_pin_mismatch(cfg->GPIO_relay_SBU1, cfg->pin_relay_SBU1) ||
                  relay_pin_mismatch(cfg->GPIO_relay_SBU2, cfg->pin_relay_SBU2);
  mismatches = mismatch ? (mismatches + 1U) : 0U;
  if (mismatches >= 2U) {
    fault_occurred(FAULT_OUTPUT_READBACK);
  }
}

bool harness_check_ignition(void) {
  bool ret = false;

  // wait until we're not reading the analog voltages anymore
  while (harness.sbu_adc_lock) {}

  switch(harness.status){
    case HARNESS_STATUS_NORMAL:
      ret = !get_gpio_input(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1);
      break;
    case HARNESS_STATUS_FLIPPED:
      ret = !get_gpio_input(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2);
      break;
    default:
      break;
  }
  return ret;
}

static uint8_t harness_detect_orientation(void) {
  uint8_t ret = harness.status;
  bool adc_fault = false;

  #ifndef BOOTSTUB
  // We can't detect orientation if the relay is being driven
  if (!harness.relay_driven) {
    harness.sbu_adc_lock = true;
    set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_ANALOG);
    set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_ANALOG);

    uint16_t sbu1_mV = adc_get_mV(&current_board->harness_config->adc_signal_SBU1);
    uint16_t sbu2_mV = adc_get_mV(&current_board->harness_config->adc_signal_SBU2);
    uint16_t sbu1_check_mV = adc_get_mV(&current_board->harness_config->adc_signal_SBU1);
    #ifdef ALLOW_DEBUG
    sbu1_check_mV += harness_adc_check_offset_mV;
    #endif
    uint16_t sbu2_check_mV = adc_get_mV(&current_board->harness_config->adc_signal_SBU2);
    uint16_t detection_threshold = current_board->avdd_mV / 2U;

    // the orientation sets the relay and CAN, so it only changes on two agreeing acquisitions
    bool agree = (ABS((int32_t)sbu1_mV - (int32_t)sbu1_check_mV) <= HARNESS_ADC_AGREEMENT_MV) &&
                 (ABS((int32_t)sbu2_mV - (int32_t)sbu2_check_mV) <= HARNESS_ADC_AGREEMENT_MV);
    if (agree) {
      harness.adc_mismatches = 0U;
      harness.sbu1_voltage_mV = sbu1_mV;
      harness.sbu2_voltage_mV = sbu2_mV;
    } else if (harness.adc_mismatches < HARNESS_ADC_MISMATCH_FAULT) {
      harness.adc_mismatches += 1U;
    } else {
      adc_fault = true;
    }

    // Detect connection and orientation
    if (agree) {
      if((harness.sbu1_voltage_mV < detection_threshold) || (harness.sbu2_voltage_mV < detection_threshold)){
        if (harness.sbu1_voltage_mV < harness.sbu2_voltage_mV) {
          // orientation flipped (PANDA_SBU1->HARNESS_SBU1(relay), PANDA_SBU2->HARNESS_SBU2(ign))
          ret = HARNESS_STATUS_FLIPPED;
        } else {
          // orientation normal (PANDA_SBU2->HARNESS_SBU1(relay), PANDA_SBU1->HARNESS_SBU2(ign))
          // (SBU1->SBU2 is the normal orientation connection per USB-C cable spec)
          ret = HARNESS_STATUS_NORMAL;
        }
      } else {
        ret = HARNESS_STATUS_NC;
      }
    }

    // Pins are not 5V tolerant in ADC mode
    set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_INPUT);
    set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_INPUT);
    harness.sbu_adc_lock = false;
  }
  #endif

  // the safe state drives the relay, which waits for the ADC lock
  if (adc_fault) {
    fault_occurred(FAULT_ADC);
  }

  return ret;
}

void harness_tick(void) {
  harness.status = harness_detect_orientation();
}

void harness_init(void) {
  harness.adc_mismatches = 0U;

  // init OBD_SBUx_RELAY
  set_gpio_output_type(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output_type(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, 1);
  set_gpio_output(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, 1);

  // detect initial orientation
  harness.status = harness_detect_orientation();

  // VREFINT derived AVDD outside the board supply range means the ADC or its reference is broken
  if (!adc_avdd_plausible(current_board->avdd_mV)) {
    fault_occurred(FAULT_ADC);
  }

  // keep buses connected by default
  set_intercept_relay(false, false);
}
