int car_harness_detected = 0;
#define HARNESS_ORIENTATION_NORMAL 1
#define HARNESS_ORIENTATION_FLIPPED 2

#define SBU1_PIN 0
#define SBU2_PIN 3

#define HARNESS_IGNITION_PIN_NORMAL (SBU2_PIN)
#define HARNESS_IGNITION_PIN_FLIPPED (SBU1_PIN)

#define HARNESS_RELAY_PIN_NORMAL 10    
#define HARNESS_RELAY_PIN_FLIPPED 11

// Threshold voltage (mV) for either of the SBUs to be below before deciding harness is connected
#define HARNESS_CONNECTED_THRESHOLD 2500

// this function will be the API for tici
bool set_intercept_relay(bool intercept) {
  if (car_harness_detected != 0) {
    if(intercept)
      puts("switching harness to intercept (relay on)\n");
    else
      puts("switching harness to passthrough (relay off)\n");

    set_gpio_output(GPIOC, (car_harness_detected == HARNESS_ORIENTATION_NORMAL) ? HARNESS_RELAY_PIN_NORMAL : HARNESS_RELAY_PIN_FLIPPED, !intercept);
  }
  return true;
}

bool harness_check_ignition(void) {
  if (car_harness_detected != 0) {
    return !get_gpio_input(GPIOC, (car_harness_detected == HARNESS_ORIENTATION_NORMAL) ? HARNESS_IGNITION_PIN_NORMAL : HARNESS_IGNITION_PIN_FLIPPED);
  }
  return false;
}

void harness_setup_ignition_interrupts(void){
  if(car_harness_detected == HARNESS_ORIENTATION_NORMAL){
    SYSCFG->EXTICR[0] = SYSCFG_EXTICR1_EXTI3_PC;
    EXTI->IMR |= (1U << 3);
    EXTI->RTSR |= (1U << 3);
    EXTI->FTSR |= (1U << 3);
    puts("setup interrupts: normal\n");
  } else if(car_harness_detected == HARNESS_ORIENTATION_FLIPPED) {
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
  int ret = 0;

  uint32_t sbu1_voltage = adc_get(ADCCHAN_SBU1);
  uint32_t sbu2_voltage = adc_get(ADCCHAN_SBU2);

  // Detect connection and orientation
  if(sbu1_voltage < HARNESS_CONNECTED_THRESHOLD || sbu2_voltage < HARNESS_CONNECTED_THRESHOLD){
    if (sbu1_voltage < sbu2_voltage) {
      // orientation normal
      ret = HARNESS_ORIENTATION_NORMAL;
    } else {
      // orientation flipped
      ret = HARNESS_ORIENTATION_FLIPPED;
    }
  }

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
    if (ret) {
      puts("detected car harness on try ");
      puth2(tries);
      puts(" with orientation ");
      puth2(ret);
      puts("\n");
      car_harness_detected = ret;

      // set the SBU lines to be inputs before using the relay. The lines are not 5V tolerant in ADC mode!
      set_gpio_mode(GPIOC, SBU1_PIN, MODE_INPUT);
      set_gpio_mode(GPIOC, SBU2_PIN, MODE_INPUT);

      // now we have orientation, set pin ignition detection
      set_gpio_mode(GPIOC, (car_harness_detected == HARNESS_ORIENTATION_NORMAL) ? HARNESS_IGNITION_PIN_NORMAL : HARNESS_IGNITION_PIN_FLIPPED, MODE_INPUT);

      // keep busses connected by default
      //set_intercept_relay(false);
      set_intercept_relay(true); // Disconnect to be backwards compatible with normal panda for testing

      // flip CAN0 and CAN2 if we are flipped
      if (car_harness_detected == HARNESS_ORIENTATION_NORMAL) {
        // flip CAN bus 0 and 2
        bus_lookup[0] = 2;
        bus_lookup[2] = 0;
        can_num_lookup[0] = 2;
        can_num_lookup[2] = 0;

        // init multiplexer
        can_set_obd(car_harness_detected, false);
      }

      // setup ignition interrupts
      harness_setup_ignition_interrupts();

      break;
    }
    delay(500000);
  }
  
  if(!car_harness_detected) puts("failed to detect car harness!\n");
  current_board->set_led(LED_RED, 0);
}