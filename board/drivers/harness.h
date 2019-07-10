int car_harness_detected = 0;
#define HARNESS_ORIENTATION_NORMAL 1
#define HARNESS_ORIENTATION_FLIPPED 2

// Threshold voltage (mV) for either of the SBUs to be below before deciding harness is connected
#define HARNESS_CONNECTED_THRESHOLD 2500

// this function will be the API for tici
bool set_can1_obd_relay(bool obd) {
  if (car_harness_detected != 0) {
    //TODO: Write relay code
    if(obd){}
    else{}
  }
  return true;
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
  set_led(LED_RED, 1);
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

      // now we have orientation, disconnect obd
      set_can1_obd_relay(false);

      // flip CAN0 and CAN2 if we are flipped
      if (car_harness_detected == HARNESS_ORIENTATION_FLIPPED) {
        // flip CAN bus 0 and 2
        // CAN bus 1 is dealt with by the relay
        bus_lookup[0] = 2;
        bus_lookup[2] = 0;
        can_num_lookup[0] = 2;
        can_num_lookup[2] = 0;
      }

      break;
    }
    delay(500000);
  }
  
  if(!car_harness_detected) puts("failed to detect car harness!\n");
  set_led(LED_RED, 0);
}