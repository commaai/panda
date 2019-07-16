int car_harness_status = 0;
#define HARNESS_STATUS_NC 0
#define HARNESS_STATUS_NORMAL 1
#define HARNESS_STATUS_FLIPPED 2

// Threshold voltage (mV) for either of the SBUs to be below before deciding harness is connected
#define HARNESS_CONNECTED_THRESHOLD 2500

#define HARNESS_GPIO GPIOC

struct harness_configuration {
  const bool has_harness;
  uint8_t pin_SBU1;
  uint8_t pin_SBU2;
  uint8_t pin_relay_normal;
  uint8_t pin_relay_flipped;
  uint8_t adc_channel_SBU1;
  uint8_t adc_channel_SBU2;
};

// this function will be the API for tici
bool set_intercept_relay(bool intercept) {
  if (car_harness_status != HARNESS_STATUS_NC) {
    if(intercept)
      puts("switching harness to intercept (relay on)\n");
    else
      puts("switching harness to passthrough (relay off)\n");

    if(car_harness_status == HARNESS_STATUS_NORMAL){
      set_gpio_output(HARNESS_GPIO, current_board->harness_config->pin_relay_normal, !intercept);
    } else {
      set_gpio_output(HARNESS_GPIO, current_board->harness_config->pin_relay_flipped, !intercept);
    }
  }
  return true;
}

bool harness_check_ignition(void) {
  switch(car_harness_status){
    case HARNESS_STATUS_NORMAL:
      return !get_gpio_input(HARNESS_GPIO, current_board->harness_config->pin_SBU2);
      break;
    case HARNESS_STATUS_FLIPPED:
      return !get_gpio_input(HARNESS_GPIO, current_board->harness_config->pin_SBU1);
      break;
    default:
      return false;
  }
}

// TODO: refactor to use harness config
void harness_setup_ignition_interrupts(void){
  if(car_harness_status == HARNESS_STATUS_NORMAL){
    SYSCFG->EXTICR[0] = SYSCFG_EXTICR1_EXTI3_PC;
    EXTI->IMR |= (1U << 3);
    EXTI->RTSR |= (1U << 3);
    EXTI->FTSR |= (1U << 3);
    puts("setup interrupts: normal\n");
  } else if(car_harness_status == HARNESS_STATUS_FLIPPED) {
    SYSCFG->EXTICR[0] = SYSCFG_EXTICR1_EXTI0_PC;
    EXTI->IMR |= (1U << 0);
    EXTI->RTSR |= (1U << 0);
    EXTI->FTSR |= (1U << 0);
    NVIC_EnableIRQ(EXTI1_IRQn);
    puts("setup interrupts: flipped\n");
  } else {
    puts("tried to setup ignition interrupts without harness connected\n");
  }
  NVIC_EnableIRQ(EXTI0_IRQn);
  NVIC_EnableIRQ(EXTI3_IRQn);
}

int harness_detect_orientation(void) {
  int ret = HARNESS_STATUS_NC;

  #ifndef BOOTSTUB
  uint32_t sbu1_voltage = adc_get(current_board->harness_config->adc_channel_SBU1);
  uint32_t sbu2_voltage = adc_get(current_board->harness_config->adc_channel_SBU2);

  // Detect connection and orientation
  if(sbu1_voltage < HARNESS_CONNECTED_THRESHOLD || sbu2_voltage < HARNESS_CONNECTED_THRESHOLD){
    if (sbu1_voltage < sbu2_voltage) {
      // orientation normal
      ret = HARNESS_STATUS_NORMAL;
    } else {
      // orientation flipped
      ret = HARNESS_STATUS_FLIPPED;
    }
  }
  #endif

  return ret;
}

void harness_init(void) {
  // chilling for power to be stable (we have interrupts)
  current_board->set_led(LED_RED, 1);
  delay(5000000);

  // on car harness, detect first
  for (int tries = 0; tries < 3; tries++) {
    puts("attempting to detect car harness...\n");
    
    int ret = harness_detect_orientation();
    if (ret != HARNESS_STATUS_NC) {
      puts("detected car harness on try ");
      puth2(tries);
      puts(" with orientation ");
      puth2(ret);
      puts("\n");
      car_harness_status = ret;

      // set the SBU lines to be inputs before using the relay. The lines are not 5V tolerant in ADC mode!
      set_gpio_mode(HARNESS_GPIO, current_board->harness_config->pin_SBU1, MODE_INPUT);
      set_gpio_mode(HARNESS_GPIO, current_board->harness_config->pin_SBU2, MODE_INPUT);

      // now we have orientation, set pin ignition detection
      if(car_harness_status == HARNESS_STATUS_NORMAL){
        set_gpio_mode(HARNESS_GPIO, current_board->harness_config->pin_SBU2, MODE_INPUT);
      } else {
        set_gpio_mode(HARNESS_GPIO, current_board->harness_config->pin_SBU1, MODE_INPUT);
      }      

      // keep busses connected by default
      //set_intercept_relay(false);
      set_intercept_relay(true); // Disconnect to be backwards compatible with normal panda for testing

      // setup ignition interrupts
      harness_setup_ignition_interrupts();

      break;
    }
    delay(500000);
  }
  
  if(car_harness_status == HARNESS_STATUS_NC) puts("failed to detect car harness!\n");
  current_board->set_led(LED_RED, 0);
}