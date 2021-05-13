// ///////////////////// //
// Red Panda + Harness //
// ///////////////////// //

void red_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  switch (transceiver){
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
      puts("Invalid CAN transceiver ("); puth(transceiver); puts("): enabling failed\n");
      break;
  }
}

void red_enable_can_transceivers(bool enabled) {
  for(uint8_t i=1U; i<=4U; i++){
    // Leave main CAN always on for CAN-based ignition detection
    if((car_harness_status == HARNESS_STATUS_FLIPPED) ? (i == 3U) : (i == 1U)){
      red_enable_can_transceiver(i, true);
    } else {
      red_enable_can_transceiver(i, enabled);
    }
  }
}

void red_set_led(uint8_t color, bool enabled) {
  switch (color){
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

void red_set_gps_load_switch(bool enabled) {
  // No GPS on red panda
  UNUSED(mode);
}

void red_set_usb_load_switch(bool enabled) {
  set_gpio_output(GPIOB, 1, !enabled);
}

void red_set_usb_power_mode(uint8_t mode) {
  bool valid = false;
  switch (mode) {
    case USB_POWER_CLIENT:
      red_set_usb_load_switch(false);
      valid = true;
      break;
    case USB_POWER_CDP:
      red_set_usb_load_switch(true);
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

void red_set_gps_mode(uint8_t mode) {
  // No GPS on red panda
  UNUSED(mode);
}

void red_set_can_mode(uint8_t mode){
  switch (mode) {
    case CAN_MODE_NORMAL:
    case CAN_MODE_OBD_CAN2:
      if ((bool)(mode == CAN_MODE_NORMAL) != (bool)(car_harness_status == HARNESS_STATUS_FLIPPED)) {
        // B12,B13: disable OBD mode
        set_gpio_mode(GPIOB, 12, MODE_INPUT);
        set_gpio_mode(GPIOB, 13, MODE_INPUT);

        // B5,B6: normal CAN2 mode
        set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);
        set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
      } else {
        // B5,B6: disable normal CAN2 mode
        set_gpio_mode(GPIOB, 5, MODE_INPUT);
        set_gpio_mode(GPIOB, 6, MODE_INPUT);

        // B12,B13: OBD mode
        set_gpio_alternate(GPIOB, 12, GPIO_AF9_FDCAN2);
        set_gpio_alternate(GPIOB, 13, GPIO_AF9_FDCAN2);
      }
      break;
    default:
      puts("Tried to set unsupported CAN mode: "); puth(mode); puts("\n");
      break;
  }
}

void red_usb_power_mode_tick(uint32_t uptime){
  UNUSED(uptime);
  // Not applicable
}

bool red_check_ignition(void){
  // ignition is checked through harness
  return harness_check_ignition();
}

uint32_t red_read_current(void){
  // No current sense on red panda
  return 0U;
}

void red_set_ir_power(uint8_t percentage){
  UNUSED(percentage);
}

void red_set_fan_power(uint8_t percentage){
  UNUSED(percentage);
}

void red_set_phone_power(bool enabled){
  UNUSED(enabled);
}

void red_set_clock_source_mode(uint8_t mode){
  UNUSED(mode);
}

void red_set_siren(bool enabled){
  UNUSED(enabled);
}

void red_init(void) {
  common_init_gpio_h7();

  // C12: OBD_SBU1 (orientation detection)
  // D0: OBD_SBU2 (orientation detection)
  set_gpio_mode(GPIOC, 12, MODE_ANALOG);
  set_gpio_mode(GPIOD, 0, MODE_ANALOG);

  // C10: OBD_SBU1_RELAY (harness relay driving output)
  // C11: OBD_SBU2_RELAY (harness relay driving output)
  set_gpio_mode(GPIOC, 10, MODE_OUTPUT);
  set_gpio_mode(GPIOC, 11, MODE_OUTPUT);
  set_gpio_output_type(GPIOC, 10, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output_type(GPIOC, 11, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output(GPIOC, 10, 1);
  set_gpio_output(GPIOC, 11, 1);

  // Turn on USB load switch.
  red_set_usb_load_switch(true);

  // Set right power mode
  red_set_usb_power_mode(USB_POWER_CDP);

  // Initialize harness
  harness_init();

  // Enable CAN transceivers
  red_enable_can_transceivers(true);

  // Disable LEDs
  red_set_led(LED_RED, false);
  red_set_led(LED_GREEN, false);
  red_set_led(LED_BLUE, false);

  // Set normal CAN mode
  red_set_can_mode(CAN_MODE_NORMAL);

  // flip CAN0 and CAN2 if we are flipped
  if (car_harness_status == HARNESS_STATUS_FLIPPED) {
    can_flip_buses(0, 2);
  }

  //TODO: check if this func is even needed !
  // init multiplexer
  can_set_obd(car_harness_status, false);
}

const harness_configuration red_harness_config = {
  .has_harness = true,
  .GPIO_SBU1 = GPIOC,
  .GPIO_SBU2 = GPIOC,
  .GPIO_relay_SBU1 = GPIOC,
  .GPIO_relay_SBU2 = GPIOC,
  .pin_SBU1 = 0,
  .pin_SBU2 = 3,
  .pin_relay_SBU1 = 10,
  .pin_relay_SBU2 = 11,
  .adc_channel_SBU1 = 10,
  .adc_channel_SBU2 = 13
};

const board board_red = {
  .board_type = "Red",
  .harness_config = &red_harness_config,
  .init = red_init,
  .enable_can_transceiver = red_enable_can_transceiver,
  .enable_can_transceivers = red_enable_can_transceivers,
  .set_led = red_set_led,
  .set_usb_power_mode = red_set_usb_power_mode,
  .set_gps_mode = red_set_gps_mode,
  .set_can_mode = red_set_can_mode,
  .usb_power_mode_tick = red_usb_power_mode_tick,
  .check_ignition = red_check_ignition,
  .read_current = red_read_current,
  .set_fan_power = red_set_fan_power,
  .set_ir_power = red_set_ir_power,
  .set_phone_power = red_set_phone_power,
  .set_clock_source_mode = red_set_clock_source_mode,
  .set_siren = red_set_siren
};
