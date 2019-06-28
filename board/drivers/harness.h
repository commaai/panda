int car_harness_detected = 0;
#define HARNESS_ORIENTATION_NORMAL 1
#define HARNESS_ORIENTATION_FLIPPED 2

#ifdef EON
#include "lin.h"
#include "uja1023.h"

#define CAN1_RELAY 1
#define CONTROLS_RELAY_NORMAL 0x40
#define CONTROLS_RELAY_FLIPPED 0x80

int _output_buffer = 0xc1;

void harness_watchdog(void) {
  if (car_harness_detected != 0) {
    //puts("harness watchdog\n");
    // don't let it go into sleep mode
    enter_critical_section();
    set_uja1023_output_buffer(_output_buffer);
    exit_critical_section();
  }
}

// this function will be the API for tici
bool set_relay_and_can1_obd(int relay, int obd) {
  if (car_harness_detected != 0) {
    int uja_output_buffer = 0xc1;   // both relays off
    if (car_harness_detected == HARNESS_ORIENTATION_NORMAL) {
      // relay
      if (relay) uja_output_buffer &= ~CONTROLS_RELAY_NORMAL;
      // can1
      if (obd) uja_output_buffer &= ~CAN1_RELAY;
    } else if (car_harness_detected == HARNESS_ORIENTATION_FLIPPED) {
      // relay
      if (relay) uja_output_buffer &= ~CONTROLS_RELAY_FLIPPED;
      // can1
      if (!obd) uja_output_buffer &= ~CAN1_RELAY;
    }
    _output_buffer = uja_output_buffer;
    harness_watchdog();
  }
  return true;
}

void harness_set_outputs_sync(int outputs) {
  LIN_FRAME_t px_req_frame;
  px_req_frame.data_len = 2;
  px_req_frame.frame_id = 0xC4; //PID, 0xC4 = 2 bit parity + 0x04 raw ID
  px_req_frame.data[0] = outputs;
  px_req_frame.data[1] = 0x80;
  uja1023_tx(&px_req_frame);
}

int harness_detect_orientation(void) {
  int ret;

  // now, what's the orientation?
  LIN_FRAME_t read_inputs_frame;
  read_inputs_frame.data_len = 2;
  read_inputs_frame.frame_id = 0x85; // read inputs
  ret = uja1023_rx(&read_inputs_frame);
  if (ret != LIN_OK) return 0;

  int cc1 = read_inputs_frame.data[0] & 4;
  int cc2 = read_inputs_frame.data[0] & 8;

  int orient = 0;

  if (!cc1 && cc2) {
    // orientation normal
    orient = 1;
  } else if (cc1 && !cc2) {
    // orientation flipped
    orient = 2;
  }

  return orient;
}

void harness_init(void) {
  // chilling for power to be stable (we have interrupts)
  set_led(LED_RED, 1);
  delay(5000000);

  // on car harness, detect first
  for (int tries = 0; tries < 3; tries++) {
    puts("attempting to detect car harness...\n");
    int ret = uja1023_init(0x61);
    if (ret) ret = harness_detect_orientation();
    if (ret) {
      puts("detected car harness on try ");
      puth2(tries);
      puts(" with orientation ");
      puth2(ret);
      puts("\n");
      car_harness_detected = ret;

      // now we have orientation, behave like giraffe
      set_relay_and_can1_obd(0, 0);

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
  set_led(LED_RED, 0);
}

#endif

