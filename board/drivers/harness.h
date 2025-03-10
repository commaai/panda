#include "harness_declarations.h"

struct harness_t harness;

// The ignition relay is only used for testing purposes
void set_intercept_relay(bool intercept, bool ignition_relay) {
  if (current_board->harness_config->has_harness) {
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
}

void set_can_mode(uint8_t mode) {
  current_board->enable_can_transceiver(2U, false);
  current_board->enable_can_transceiver(4U, false);
  switch (mode) {
    case CAN_MODE_NORMAL:
    case CAN_MODE_OBD_CAN2:
      if ((bool)(mode == CAN_MODE_NORMAL) != (bool)(harness.status == HARNESS_STATUS_FLIPPED)) {
        // normal pins
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_RX_NORMAL, current_board->harness_config->pin_CAN2_RX_NORMAL, MODE_ANALOG);
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_TX_NORMAL, current_board->harness_config->pin_CAN2_TX_NORMAL, MODE_ANALOG);

        // flipped pins
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_RX_FLIPPED, current_board->harness_config->pin_CAN2_RX_FLIPPED, GPIO_CAN2_AF);
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_TX_FLIPPED, current_board->harness_config->pin_CAN2_TX_FLIPPED, GPIO_CAN2_AF);
        current_board->enable_can_transceiver(2U, true);
      } else {
        // normal pins
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_RX_FLIPPED, current_board->harness_config->pin_CAN2_RX_FLIPPED, MODE_ANALOG);
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_TX_FLIPPED, current_board->harness_config->pin_CAN2_TX_FLIPPED, MODE_ANALOG);

        // flipped pins
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_RX_NORMAL, current_board->harness_config->pin_CAN2_RX_NORMAL, GPIO_CAN2_AF);
        set_gpio_mode(current_board->harness_config->GPIO_CAN2_TX_NORMAL, current_board->harness_config->pin_CAN2_TX_NORMAL, GPIO_CAN2_AF);
        current_board->enable_can_transceiver(4U, true);
      }
      break;
    default:
      break;
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

  #ifndef BOOTSTUB
  // We can't detect orientation if the relay is being driven
  if (!harness.relay_driven && current_board->harness_config->has_harness) {
    harness.sbu_adc_lock = true;
    set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_ANALOG);
    set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_ANALOG);

    harness.sbu1_voltage_mV = adc_get_mV(current_board->harness_config->adc_channel_SBU1);
    harness.sbu2_voltage_mV = adc_get_mV(current_board->harness_config->adc_channel_SBU2);
    uint16_t detection_threshold = current_board->avdd_mV / 2U;

    // Detect connection and orientation
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

    // Pins are not 5V tolerant in ADC mode
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
  set_can_mode(CAN_MODE_NORMAL);

  // init OBD_SBUx_RELAY
  set_gpio_output_type(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output_type(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output(current_board->harness_config->GPIO_relay_SBU1, current_board->harness_config->pin_relay_SBU1, 1);
  set_gpio_output(current_board->harness_config->GPIO_relay_SBU2, current_board->harness_config->pin_relay_SBU2, 1);

  // try to detect orientation
  harness.status = harness_detect_orientation();
  if (harness.status != HARNESS_STATUS_NC) {
    print("detected car harness with orientation "); puth2(harness.status); print("\n");
  } else {
    print("failed to detect car harness!\n");
  }

  // keep buses connected by default
  set_intercept_relay(false, false);
}
