#define PZILLA_MASTER 0U
#define PZILLA_SLAVE_1 1U
#define PZILLA_SLAVE_2 2U

uint8_t pzilla_role = 0U;


void pzilla_enable_can_transceivers(bool enabled) {
  for(uint8_t i=1U; i<=3U; i++){
    // Leave main CAN always on for CAN-based ignition detection
    if(i == 1U){
      black_enable_can_transceiver(i, true);
    } else {
      black_enable_can_transceiver(i, enabled);
    }
  }
}

void pzilla_set_can_mode(uint8_t mode){
  switch (mode) {
    case CAN_MODE_NORMAL:
    case CAN_MODE_OBD_CAN2:
      // B5,B6: normal CAN2 mode
      set_gpio_alternate(GPIOB, 5, GPIO_AF9_CAN2);
      set_gpio_alternate(GPIOB, 6, GPIO_AF9_CAN2);
      break;
    default:
      print("Tried to set unsupported CAN mode: "); puth(mode); print("\n");
      break;
  }
}

void pzilla_set_usb_load_switch(bool enabled) {
  set_gpio_output(GPIOB, 1, enabled);
}

void pzilla_set_phone_power(bool enabled) {
  set_gpio_output(GPIOD, 2, enabled);
}

uint8_t pzilla_detect_role(void) {
  return detect_with_pull(GPIOC, 11, PULL_UP) | (detect_with_pull(GPIOC, 12, PULL_UP) << 1U);
}

void pzilla_init(void) {
  common_init_gpio();

  pzilla_role = pzilla_detect_role();

  if (pzilla_role == PZILLA_MASTER) {
  // Turn on USB load switch.
  pzilla_set_usb_load_switch(true);

  // Disable powering USB device from internal regulator
  pzilla_set_phone_power(false);
  }

  // A8,A15: normal CAN3 mode
  set_gpio_alternate(GPIOA, 8, GPIO_AF11_CAN3);
  set_gpio_alternate(GPIOA, 15, GPIO_AF11_CAN3);

  // For USB current sensing
  set_gpio_mode(GPIOC, 3, MODE_ANALOG);

  // C5: OBD_SBU1 (orientation detection)
  // C0: OBD_SBU2 (orientation detection)
  set_gpio_mode(GPIOC, 0, MODE_ANALOG);
  set_gpio_mode(GPIOC, 5, MODE_ANALOG);

  // C10: OBD_SBU2_RELAY (harness relay driving output)
  set_gpio_mode(GPIOC, 10, MODE_OUTPUT);
  set_gpio_output_type(GPIOC, 10, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output(GPIOC, 10, 1);

  // set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_INPUT);

  // Initialize harness
  harness_init();

  // Set to fake ignition on SBU1 to false as we don't have ignition line
  set_gpio_pullup(GPIOC, 5, PULL_UP); // Should be changed to PC3 on next revision
  // Set normal CAN mode
  pzilla_set_can_mode(CAN_MODE_NORMAL);
  // car_harness_status = HARNESS_STATUS_NORMAL;

  // Initialize RTC
  rtc_init();

  // Enable CAN transceivers
  pzilla_enable_can_transceivers(true);

  // Disable LEDs
  black_set_led(LED_RED, false);
  black_set_led(LED_GREEN, false);
  black_set_led(LED_BLUE, false);
}

const harness_configuration pzilla_harness_config = {
  .has_harness = true,
  .GPIO_SBU1 = GPIOC,
  .GPIO_SBU2 = GPIOC,
  .GPIO_relay_SBU1 = GPIOC,
  .GPIO_relay_SBU2 = GPIOC,
  .pin_SBU1 = 5, // Set to unused pin PC5
  .pin_SBU2 = 0,
  .pin_relay_SBU1 = 10, // Duplicate SBU2 relay as we don't have SBU1
  .pin_relay_SBU2 = 10,
  .adc_channel_SBU1 = 15, // Set to unused channel 15
  .adc_channel_SBU2 = 10
};

const board board_pzilla = {
  .board_type = "Pandazilla",
  .board_tick = unused_board_tick,
  .harness_config = &pzilla_harness_config,
  .has_gps = false,
  .has_hw_gmlan = false,
  .has_obd = false,
  .has_lin = false,
  .has_spi = false,
  .has_canfd = false,
  .has_rtc_battery = false,
  .fan_max_rpm = 0U,
  .init = pzilla_init,
  .enable_can_transceiver = black_enable_can_transceiver,
  .enable_can_transceivers = pzilla_enable_can_transceivers,
  .set_led = black_set_led,
  .set_gps_mode = unused_set_gps_mode,
  .set_can_mode = pzilla_set_can_mode,
  .check_ignition = black_check_ignition,
  .read_current = unused_read_current,
  .set_fan_enabled = unused_set_fan_enabled,
  .set_ir_power = unused_set_ir_power,
  .set_phone_power = unused_set_phone_power,
  .set_clock_source_mode = unused_set_clock_source_mode,
  .set_siren = unused_set_siren
};
