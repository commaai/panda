// ////////////// //
// Tres + Harness //
// ////////////// //

void tres_enable_can_transceiver(uint8_t transceiver, bool enabled) {
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

void tres_enable_can_transceivers(bool enabled) {
  uint8_t main_bus = (car_harness_status == HARNESS_STATUS_FLIPPED) ? 3U : 1U;
  for (uint8_t i=1U; i<=4U; i++) {
    // Leave main CAN always on for CAN-based ignition detection
    if (i == main_bus) {
      red_enable_can_transceiver(i, true);
    } else {
      red_enable_can_transceiver(i, enabled);
    }
  }
}

void tres_set_led(uint8_t color, bool enabled) {
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

void tres_set_bootkick(bool enabled){
  set_gpio_output(GPIOA, 0, !enabled);
}

void tres_set_usb_power_mode(uint8_t mode) {
  bool valid = false;
  switch (mode) {
    case USB_POWER_CLIENT:
      tres_set_bootkick(false);
      valid = true;
      break;
    case USB_POWER_CDP:
      tres_set_bootkick(true);
      valid = true;
      break;
    default:
      puts("Invalid USB power mode\n");
      break;
  }
  if (valid) {
    usb_power_mode = mode;
  }
}

void tres_set_can_mode(uint8_t mode) {
  switch (mode) {
    case CAN_MODE_NORMAL:
    case CAN_MODE_OBD_CAN2:
      if ((bool)(mode == CAN_MODE_NORMAL) != (bool)(car_harness_status == HARNESS_STATUS_FLIPPED)) {
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
      }
      break;
    default:
      break;
  }
}

bool tres_check_ignition(void) {
  // ignition is checked through harness
  return harness_check_ignition();
}

void tres_set_ir_power(uint8_t percentage){
  pwm_set(TIM4, 2, percentage);
}

void tres_set_clock_source_mode(uint8_t mode){
  // TODO: rewrite for H7
  UNUSED(mode);
  // clock_source_init(mode);
}

void tres_set_siren(bool enabled){
  set_gpio_output(GPIOA, 10, enabled);
}

void tres_set_fan_power(uint8_t percentage){
  // Enable fan power only if percentage is non-zero.
  set_gpio_output(GPIOD, 3, (percentage != 0U));
  fan_set_power(percentage);
}

void tres_init(void) {
  common_init_gpio();

  //C4,A1: OBD_SBU1, OBD_SBU2
  set_gpio_pullup(GPIOC, 4, PULL_NONE);
  set_gpio_mode(GPIOC, 4, MODE_ANALOG);

  set_gpio_pullup(GPIOA, 1, PULL_NONE);
  set_gpio_mode(GPIOA, 1, MODE_ANALOG);

  //A8,A9 : OBD_SBU1_RELAY, OBD_SBU2_RELAY
  set_gpio_output_type(GPIOA, 8, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_pullup(GPIOA, 8, PULL_NONE);
  set_gpio_mode(GPIOA, 8, MODE_OUTPUT);
  set_gpio_output(GPIOA, 8, 1);

  set_gpio_output_type(GPIOA, 9, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_pullup(GPIOA, 9, PULL_NONE);
  set_gpio_mode(GPIOA, 9, MODE_OUTPUT);
  set_gpio_output(GPIOA, 9, 1);

  // C8: FAN PWM aka TIM3_CH3
  set_gpio_alternate(GPIOC, 8, GPIO_AF2_TIM3);

  // Initialize IR PWM and set to 0%
  set_gpio_alternate(GPIOB, 7, GPIO_AF2_TIM4);
  pwm_init(TIM4, 2);
  tres_set_ir_power(0U);

  // Initialize fan and set to 0%
  fan_init();
  tres_set_fan_power(0U);

  // Initialize harness
  harness_init();

  // Initialize RTC
  rtc_init();

  // Enable CAN transceivers
  tres_enable_can_transceivers(true);

  // Disable LEDs
  tres_set_led(LED_RED, false);
  tres_set_led(LED_GREEN, false);
  tres_set_led(LED_BLUE, false);

  // Set normal CAN mode
  tres_set_can_mode(CAN_MODE_NORMAL);

  // flip CAN0 and CAN2 if we are flipped
  if (car_harness_status == HARNESS_STATUS_FLIPPED) {
    can_flip_buses(0, 2);
  }

  // Init clock source (camera strobe) using PWM TODO
  // tres_set_clock_source_mode(CLOCK_SOURCE_MODE_PWM);
}

const harness_configuration tres_harness_config = {
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
  .adc_channel_SBU2 = 17 //ADC1_INP17
};

const board board_tres = {
  .board_type = "Tres",
  .harness_config = &tres_harness_config,
  .has_gps = false,
  .has_hw_gmlan = false,
  .has_obd = true,
  .has_lin = false,
  .has_rtc_battery = true,
  .init = tres_init,
  .enable_can_transceiver = tres_enable_can_transceiver,
  .enable_can_transceivers = tres_enable_can_transceivers,
  .set_led = tres_set_led,
  .set_usb_power_mode = tres_set_usb_power_mode,
  .set_gps_mode = unused_set_gps_mode,
  .set_can_mode = tres_set_can_mode,
  .usb_power_mode_tick = unused_usb_power_mode_tick,
  .check_ignition = tres_check_ignition,
  .read_current = unused_read_current,
  .set_fan_power = tres_set_fan_power,
  .set_ir_power = tres_set_ir_power,
  .set_phone_power = unused_set_phone_power,
  .set_clock_source_mode = tres_set_clock_source_mode,
  .set_siren = tres_set_siren
};
