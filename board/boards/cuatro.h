// ////////////////////////// //
// Cuatro (STM32H7) + Harness //
// ////////////////////////// //

void cuatro_set_led(uint8_t color, bool enabled) {
  // try no LED
  UNUSED(enabled);
  switch (color) {
    case LED_RED:
      set_gpio_output(GPIOD, 15, 1);
      break;
     case LED_GREEN:
      set_gpio_output(GPIOD, 14, 1);
      break;
    case LED_BLUE:
      set_gpio_output(GPIOE, 2, 1);
      break;
    default:
      break;
  }
}

void cuatro_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  switch (transceiver) {
    case 1U:
      set_gpio_output(GPIOB, 7, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOB, 10, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOD, 8, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 11, !enabled);
      break;
    default:
      break;
  }
}

void cuatro_enable_can_transceivers(bool enabled) {
  uint8_t main_bus = (harness.status == HARNESS_STATUS_FLIPPED) ? 3U : 1U;
  for (uint8_t i=1U; i<=4U; i++) {
    // Leave main CAN always on for CAN-based ignition detection
    if (i == main_bus) {
      cuatro_enable_can_transceiver(i, true);
    } else {
      cuatro_enable_can_transceiver(i, enabled);
    }
  }
}

uint32_t cuatro_read_voltage_mV(void) {
  return adc_get_mV(8) * 11U;
}

uint32_t cuatro_read_current_mA(void) {
  return adc_get_mV(3) * 2U;
}

void cuatro_set_fan_enabled(bool enabled) {
  set_gpio_output(GPIOD, 3, !enabled);
}

void cuatro_set_bootkick(BootState state) {
  set_gpio_output(GPIOA, 0, state != BOOT_BOOTKICK);
  // only use if we have to
  //set_gpio_output(GPIOC, 12, state != BOOT_RESET);
}

void cuatro_init(void) {
  red_chiplet_init();

  // LED init
  set_gpio_pullup(GPIOD, 14, PULL_NONE);
  set_gpio_mode(GPIOD, 14, MODE_OUTPUT);
  set_gpio_output_type(GPIOD, 14, OUTPUT_TYPE_OPEN_DRAIN);

  set_gpio_pullup(GPIOD, 15, PULL_NONE);
  set_gpio_mode(GPIOD, 15, MODE_OUTPUT);
  set_gpio_output_type(GPIOD, 15, OUTPUT_TYPE_OPEN_DRAIN);

  set_gpio_pullup(GPIOE, 2, PULL_NONE);
  set_gpio_mode(GPIOE, 2, MODE_OUTPUT);
  set_gpio_output_type(GPIOE, 2, OUTPUT_TYPE_OPEN_DRAIN);

  // Power readout
  set_gpio_mode(GPIOC, 5, MODE_ANALOG);
  set_gpio_mode(GPIOA, 6, MODE_ANALOG);

  // CAN transceiver enables
  set_gpio_pullup(GPIOB, 7, PULL_NONE);
  set_gpio_mode(GPIOB, 7, MODE_OUTPUT);
  set_gpio_pullup(GPIOD, 8, PULL_NONE);
  set_gpio_mode(GPIOD, 8, MODE_OUTPUT);

  // FDCAN3, different pins on this package than the rest of the reds
  set_gpio_pullup(GPIOD, 12, PULL_NONE);
  set_gpio_alternate(GPIOD, 12, GPIO_AF5_FDCAN3);
  set_gpio_pullup(GPIOD, 13, PULL_NONE);
  set_gpio_alternate(GPIOD, 13, GPIO_AF5_FDCAN3);

  // C2: SOM GPIO used as input (fan control at boot)
  set_gpio_mode(GPIOC, 2, MODE_INPUT);
  set_gpio_pullup(GPIOC, 2, PULL_DOWN);

  // SOM bootkick + reset lines
  cuatro_set_bootkick(BOOT_BOOTKICK);

  // SOM debugging UART
  gpio_uart7_init();
  uart_init(&uart_ring_som_debug, 115200);

  // SPI init
  gpio_spi_init();

  // fan setup
  set_gpio_alternate(GPIOC, 8, GPIO_AF2_TIM3);
  register_set_bits(&(GPIOC->OTYPER), GPIO_OTYPER_OT8); // open drain

  // Initialize IR PWM and set to 0%
  set_gpio_alternate(GPIOC, 9, GPIO_AF2_TIM3);
  pwm_init(TIM3, 4);
  tres_set_ir_power(0U);

  // Clock source
  clock_source_init();
}

board board_cuatro = {
  .harness_config = &red_chiplet_harness_config,
  .has_obd = true,
  .has_spi = true,
  .has_canfd = true,
  .fan_max_rpm = 12500U,
  .fan_max_pwm = 99U, // it can go up to 14k RPM, but 99% -> 100% is very non-linear
  .avdd_mV = 1800U,
  .fan_stall_recovery = false,
  .fan_enable_cooldown_time = 3U,
  .init = cuatro_init,
  .init_bootloader = unused_init_bootloader,
  .enable_can_transceiver = cuatro_enable_can_transceiver,
  .enable_can_transceivers = cuatro_enable_can_transceivers,
  .set_led = cuatro_set_led,
  .set_can_mode = red_chiplet_set_can_mode,
  .check_ignition = red_check_ignition,
  .read_voltage_mV = cuatro_read_voltage_mV,
  .read_current_mA = cuatro_read_current_mA,
  .set_fan_enabled = cuatro_set_fan_enabled,
  .set_ir_power = tres_set_ir_power,
  .set_siren = unused_set_siren,
  .set_bootkick = cuatro_set_bootkick,
  .read_som_gpio = tres_read_som_gpio
};
