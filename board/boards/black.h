// ///////////////////// //
// Black Panda + Harness //
// ///////////////////// //

void black_enable_can_transciever(uint8_t transciever, bool enabled) {
  switch (transciever){
    case 1U:
      set_gpio_output(GPIOC, 1, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOC, 13, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOA, 0, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 10, !enabled);
      break;
    default:
      puts("Invalid CAN transciever ("); puth(transciever); puts("): enabling failed\n");
      break;
  }
}

void black_enable_can_transcievers(bool enabled) {
  for(uint8_t i=1; i<=4; i++)
    black_enable_can_transciever(i, enabled);
}

void black_set_led(uint8_t color, bool enabled) {
  switch (color){
    case LED_RED:
      set_gpio_output(GPIOC, 9, !enabled);
      break;
     case LED_GREEN:
      set_gpio_output(GPIOC, 7, !enabled);
      break;
    case LED_BLUE:
      set_gpio_output(GPIOC, 6, !enabled);
      break;  
    default:
      break;
  }
}

void black_set_usb_power_mode(uint8_t mode){
  usb_power_mode = mode;
  puts("Trying to set USB power mode on black panda. This is not supported.\n");
}

void black_set_esp_gps_mode(uint8_t mode) {
  switch (mode) {
    case ESP_GPS_DISABLED:
      // ESP OFF
      set_gpio_output(GPIOC, 14, 0);
      set_gpio_output(GPIOC, 5, 0);
      break;
    case ESP_GPS_ENABLED:
      // ESP ON
      set_gpio_output(GPIOC, 14, 1);
      set_gpio_output(GPIOC, 5, 1);
      break;
    case ESP_GPS_BOOTMODE:
      set_gpio_output(GPIOC, 14, 1);
      set_gpio_output(GPIOC, 5, 0);
      break;
    default:
      puts("Invalid ESP/GPS mode\n");
      break;
  }
}

void black_set_can_mode(uint8_t mode){
  switch (mode) {
    case CAN_MODE_NORMAL:
      // TODO: Implement with harness orientation
      break;
    case CAN_MODE_OBD_CAN2:
      // TODO: Implement with harness orientation
      break;
    default:
      puts("Tried to set unsupported CAN mode: "); puth(mode); puts("\n");
      break;
  }
}

void black_init(void) {
  common_init_gpio();

  // C0: OBD_SBU1 (orientation detection)
  // C3: OBD_SBU2 (orientation detection)
  set_gpio_mode(GPIOC, 0, MODE_ANALOG);
  set_gpio_mode(GPIOC, 3, MODE_ANALOG);

  // C10: OBD_SBU1_RELAY (harness relay driving output)
  // C11: OBD_SBU2_RELAY (harness relay driving output)
  set_gpio_mode(GPIOC, 10, MODE_OUTPUT);
  set_gpio_mode(GPIOC, 11, MODE_OUTPUT);
  set_gpio_output_type(GPIOC, 10, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output_type(GPIOC, 11, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_output(GPIOC, 10, 1);
  set_gpio_output(GPIOC, 11, 1);

  // C8: FAN aka TIM3_CH3
  set_gpio_alternate(GPIOC, 8, GPIO_AF2_TIM3);

  // C12: GPS load switch. Turn on permanently for now
  //set_gpio_output(GPIOC, 12, true);
  set_gpio_output(GPIOC, 12, false); //TODO: stupid inverted switch on prototype

  // Initialize harness
  harness_init();

  // Enable CAN transcievers
  black_enable_can_transcievers(true);

  // Disable LEDs
  black_set_led(LED_RED, false);
  black_set_led(LED_GREEN, false);
  black_set_led(LED_BLUE, false);

  // Set normal CAN mode
  black_set_can_mode(CAN_MODE_NORMAL);

  // flip CAN0 and CAN2 if we are flipped
  // TODO: implement
  /*if (car_harness_status == HARNESS_STATUS_NORMAL) {
    // flip CAN bus 0 and 2
    bus_lookup[0] = 2;
    bus_lookup[2] = 0;
    can_num_lookup[0] = 2;
    can_num_lookup[2] = 0;

    // init multiplexer
    can_set_obd(car_harness_status, false);
  }*/
}

const harness_configuration black_harness_config = {
  .has_harness = true,
  .pin_SBU1 = 0,
  .pin_SBU2 = 3,
  .pin_relay_normal = 10,
  .pin_relay_flipped = 11,
  .adc_channel_SBU1 = 10,
  .adc_channel_SBU2 = 13
};

const board board_black = {
  .board_type = "Black",
  .harness_config = &black_harness_config,
  .init = black_init,
  .enable_can_transciever = black_enable_can_transciever,
  .enable_can_transcievers = black_enable_can_transcievers,
  .set_led = black_set_led,
  .set_usb_power_mode = black_set_usb_power_mode,
  .set_esp_gps_mode = black_set_esp_gps_mode
};