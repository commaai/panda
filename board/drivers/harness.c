#include "board/config.h"
#include "board/drivers/harness.h"

struct harness_t harness;

void set_intercept_relay(bool intercept, bool ignition_relay) {
  bool drive_relay = intercept;
  if (harness.status == HARNESS_STATUS_NC) {
    drive_relay = false;
  }

  if (drive_relay || ignition_relay) {
    harness.relay_driven = true;
  }

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

bool harness_check_ignition(void) {
  bool ret = false;

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

  #ifndef BOOTSTUB
  if (!harness.relay_driven) {
    harness.sbu_adc_lock = true;
    set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_ANALOG);
    set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_ANALOG);

    harness.sbu1_voltage_mV = adc_get_mV(&current_board->harness_config->adc_signal_SBU1);
    harness.sbu2_voltage_mV = adc_get_mV(&current_board->harness_config->adc_signal_SBU2);
    uint16_t detection_threshold = current_board->avdd_mV / 2U;

    if((harness.sbu1_voltage_mV < detection_threshold) || (harness.sbu2_voltage_mV < detection_threshold)){
      if (harness.sbu1_voltage_mV < harness.sbu2_voltage_mV) {
        ret = HARNESS_STATUS_FLIPPED;
      } else {
        ret = HARNESS_STATUS_NORMAL;
      }
    } else {
      ret = HARNESS_STATUS_NC;
    }

    set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_INPUT);
    set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_INPUT);
    harness.sbu_adc_lock = false;
  }
  #endif

  return ret;
}

void harness_tick(void) {
  harness.status = harness_detect_orientation();
}

void harness_init(void) {
  set_gpio_output_type(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output_type(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, 1);
  set_gpio_output(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, 1);

  harness.status = harness_detect_orientation();

  set_intercept_relay(false, false);
}
