uint8_t car_harness_status = 0U;
#define HARNESS_STATUS_NC 0U
#define HARNESS_STATUS_NORMAL 1U
#define HARNESS_STATUS_FLIPPED 2U
// NOTE: Because of an inconsistency in the OBD-C pinouts of panda and harness:
// HARNESS_STATUS_NORMAL means the USB-C cable is actually flipped and PANDA_SBU1->HARNESS_SBU2, PANDA_SBU2->HARNESS_SBU1
//   but PANDA_CAN0->HARNESS_CAN0, PANDA_CAN1->HARNESS_CAN1, PANDA_CAN2->HARNESS_CAN2, PANDA_CAN3->HARNESS_CAN3
// HARNESS_STATUS_FLIPPED means the USB-C cable is actually normal and PANDA_SBU1->HARNESS_SBU1, PANDA_SBU2->HARNESS_SBU2
//   but PANDA_CAN0->HARNESS_CAN3, PANDA_CAN1->HARNESS_CAN2, PANDA_CAN2->HARNESS_CAN1, PANDA_CAN3->HARNESS_CAN0
// So NORMAL and FLIPPED describes whether the CAN connections are flipped, not the cable

// Threshold voltage (mV) for either of the SBUs to be below before deciding harness is connected
#define HARNESS_CONNECTED_THRESHOLD 2500U

struct harness_configuration {
  const bool has_harness;
  GPIO_TypeDef *GPIO_SBU1;
  GPIO_TypeDef *GPIO_SBU2;
  GPIO_TypeDef *GPIO_relay_normal;
  GPIO_TypeDef *GPIO_relay_flipped;
  uint8_t pin_SBU1;
  uint8_t pin_SBU2;
  uint8_t pin_relay_normal;
  uint8_t pin_relay_flipped;
  uint8_t adc_channel_SBU1;
  uint8_t adc_channel_SBU2;
};

// this function will be the API for tici
void set_intercept_relay(bool intercept) {
  if (car_harness_status != HARNESS_STATUS_NC) {
    if (intercept) {
      puts("switching harness to intercept (relay on)\n");
    } else {
      puts("switching harness to passthrough (relay off)\n");
    }

    if(car_harness_status == HARNESS_STATUS_NORMAL){
      set_gpio_output(current_board->harness_config->GPIO_relay_normal, current_board->harness_config->pin_relay_normal, !intercept);
    } else {
      set_gpio_output(current_board->harness_config->GPIO_relay_flipped, current_board->harness_config->pin_relay_flipped, !intercept);
    }
  }
}

bool harness_check_ignition(void) {
  bool ret = false;
  switch(car_harness_status){
    case HARNESS_STATUS_NORMAL:
      ret = !get_gpio_input(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2);
      break;
    case HARNESS_STATUS_FLIPPED:
      ret = !get_gpio_input(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1);
      break;
    default:
      break;
  }
  return ret;
}

uint8_t harness_detect_orientation(void) {
  uint8_t ret = HARNESS_STATUS_NC;

  #ifndef BOOTSTUB
  uint32_t sbu1_voltage = adc_get(current_board->harness_config->adc_channel_SBU1);
  uint32_t sbu2_voltage = adc_get(current_board->harness_config->adc_channel_SBU2);

  // Detect connection and orientation
  if((sbu1_voltage < HARNESS_CONNECTED_THRESHOLD) || (sbu2_voltage < HARNESS_CONNECTED_THRESHOLD)){
    if (sbu1_voltage < sbu2_voltage) {
      // orientation normal (see note above)
      ret = HARNESS_STATUS_NORMAL;
    } else {
      // orientation flipped (see note above)
      ret = HARNESS_STATUS_FLIPPED;
    }
  }
  #endif

  return ret;
}

void harness_init(void) {
  // delay such that the connection is fully made before trying orientation detection
  current_board->set_led(LED_BLUE, true);
  delay(10000000);
  current_board->set_led(LED_BLUE, false);

  // try to detect orientation
  uint8_t ret = harness_detect_orientation();
  if (ret != HARNESS_STATUS_NC) {
    puts("detected car harness with orientation "); puth2(ret); puts("\n");
    car_harness_status = ret;

    // set the SBU lines to be inputs before using the relay. The lines are not 5V tolerant in ADC mode!
    set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_INPUT);
    set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_INPUT);

    // now we have orientation, set pin ignition detection
    if(car_harness_status == HARNESS_STATUS_NORMAL){
      set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_INPUT);
    } else {
      set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_INPUT);
    }

    // keep busses connected by default
    set_intercept_relay(false);
  } else {
    puts("failed to detect car harness!\n");
  }
}
