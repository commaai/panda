//#define DEBUG_VOLVO

#ifdef DEBUG_VOLVO
int giraffe_forward_camera_volvo_prev = 0;
bool controls_allowed_prev_v = 0;
bool relay_malfunction_prev = false;
bool valid_prev = false;
bool ok_to_send_prev = false;
int tx_prev = 0;
int counterloop = 0;
#endif

/*
Volvo Electronic Control Units abbreviations and network topology
Platforms C1/EUCD

Look in selfdrive/car/volvo/values.py for more information.
*/

// Globals
int giraffe_forward_camera_volvo = 0;
int acc_active_prev_volvo = 0;
int acc_ped_val_prev = 0;
int volvo_desired_angle_last = 0;
float volvo_speed = 0;

// diagnostic msgs
#define MSG_DIAG_CEM 0x726
#define MSG_DIAG_PSCM 0x730
#define MSG_DIAG_FSM 0x764
#define MSG_DIAG_CVM 0x793
#define MSG_DIAG_BROADCAST 0x7df

// platform C1
// msg ids
#define MSG_BTNS_VOLVO_C1 0x10 // Steering wheel buttons
#define MSG_FSM0_VOLVO_C1 0x30 // ACC status message
#define MSG_FSM1_VOLVO_C1 0xd0 // LKA steering message
#define MSG_FSM2_VOLVO_C1 0x160
#define MSG_FSM3_VOLVO_C1 0x270
#define MSG_FSM4_VOLVO_C1 0x280
#define MSG_FSM5_VOLVO_C1 0x355
#define MSG_PSCM0_VOLVO_C1 0xe0
#define MSG_PSCM1_VOLVO_C1 0x125    // Steering
#define MSG_ACC_PEDAL_VOLVO_C1 0x55 // Gas pedal
#define MSG_SPEED_VOLVO_C1 0x130    // Speed signal

// platform eucd
// msg ids
#define MSG_FSM0_VOLVO_V60 0x51  // ACC status message
#define MSG_FSM1_VOLVO_V60 0x260
#define MSG_FSM2_VOLVO_V60 0x262 // LKA steering message
#define MSG_FSM3_VOLVO_V60 0x270
#define MSG_FSM4_VOLVO_V60 0x31a
#define MSG_FSM5_VOLVO_V60 0x3fd
#define MSG_PSCM1_VOLVO_V60 0x246
#define MSG_ACC_PEDAL_VOLVO_V60 0x20 // Gas pedal
#define MSG_BTNS_VOLVO_V60 0x127     // Steering wheel buttons

// safety params
const float DEG_TO_CAN_VOLVO_C1 = 1/0.04395;            // 22.75312855517634‬, inverse of dbc scaling
const int VOLVO_MAX_DELTA_OFFSET_ANGLE = 20/0.04395-1;  // max degrees divided by k factor in dbc 0.04395. -1 to give a little safety margin. 
                                                        // 25 degrees allowed, more will trigger disengage by servo.
const int VOLVO_MAX_ANGLE_REQ = 8189;                   // max, min angle req, set at 2steps from max and min values.
const int VOLVO_MIN_ANGLE_REQ = -8190;                  // 14 bits long, min -8192 -> 8191.

const struct lookup_t VOLVO_LOOKUP_ANGLE_RATE_UP = {
  {7., 17., 36.},  // 25.2, 61.2, 129.6 km/h
  {2, .25, .1}
};
const struct lookup_t VOLVO_LOOKUP_ANGLE_RATE_DOWN = {
  {7., 17., 36.},
  {2, .25, .1}
};

struct sample_t volvo_angle_meas;  // last 3 steer angles

/* 
// saved for documentation purpose
// allowed messages to forward from bus 0 -> 2
const int ALLOWED_MSG_C1[] = {
0x8,      // SAS
0x10,     // CEM
0x40,     // TCM
0x55,     // ECM, Test failed ACC seems to work, when standing still
//0x65,
//0x70,
0x72,     // ECM, Test failed ACC seems to work, when standing still
0x75,     // ECM
//0x80,
0xb0,     // ECM
//0xc0,
0xe0,     // PSCM 
//0xf0,
0xf5,     // BCM
//0x100,
//0x110,
0x120,    // BCM
//0x123,
//0x125,  // PSCM - Do not forward. FSM sets fault code if it sees LKAActive and LKATorque when not requesting steering,
          //        Forwarding and manipulation of bits done in carcontroller.py
0x130,    // BCM critical for ACC 
0x145,    // ECM critical for ACC
0x150,    // BCM
//0x1a8,
0x1b0,    // BCM
0x1b5,    // BCM
0x1d0,    // DIM, Infotainment A , ACC ok, blis lka fcw off.
//0x1d8,
0x1e0,    // BCM , blis fcw nok.
0x210,    // CEM, Infotainment A, ACC ok, lka blis fcw off. 
0x260,    // ECM (When disconnecting CVM this message disappears. Why fault code for ECM?)
0x288,    // SRS 
0x28c,    // ECM
0x290,    // ECM
0x291,    // ECM
0x2a9,    // CEM, not critical 
0x2b5,    // ECM 
//0x2c0,
//0x2c3,
0x2c5,    // ECM
//0x330,
//0x340,
//0x350,
0x360,    // CEM
//0x370,  
0x390,    // CEM, DIM 
//0x3a0,
//0x3af,
//0x3b0,
0x3c8,    // SRS
//0x3ca, 
//0x3d0, 
//0x3e0, 
//0x3e5,
//0x400, 
0x405,    // CEM, BCM, CanBus System Program failure, C0 01 55,
//0x425,   
//0x430,  
//0x581,   
0x764,    // Diagnostic messages 
0x7df,    // Diagnostic messages
}; */

const int ALLOWED_MSG_EUCD[] = {
0x10, // SAS
0x20,
0x63,
0x68,
0x70,
0x90,
0x115,
0x127,
0x12A,
0x133,
0x140,
0x148,
0x150,
0x157,
0x160,
0x167,
0x180,
0x1D1,
0x1FF,
0x20A,
0x220,
0x235,
//0x246, PSCM1 
0x261,
0x264,
0x265,
0x272,
0x27B,
0x288,
0x299,
0x2A1,
0x2C0,
0x2C2,
0x2C4,
0x2EE,
0x2EF,
0x30A,
0x314,
0x31D,
0x322,
0x323,
0x325,
0x327,
0x333,
0x334,
0x335,
0x391,
0x39B,
0x3D2,
0x3D3,
0x3EE,
0x400,
0x405,
0x40F,
0x412,
0x415,
0x471,
0x475,
0x480,
0x496,
0x4A3,
0x4AE,
0x4BE,
0x4C1,
0x4CA,
0x4D8,
0x4FF,
0x581,
0x764, // Diagnostic messages
0x7df, // Diagnostic messages
};


//const int ALLOWED_MSG_C1_LEN = sizeof(ALLOWED_MSG_C1) / sizeof(ALLOWED_MSG_C1[0]);
const int ALLOWED_MSG_EUCD_LEN = sizeof(ALLOWED_MSG_EUCD) / sizeof(ALLOWED_MSG_EUCD[0]);

// TX checks
// platform c1
const CanMsg VOLVO_C1_TX_MSGS[] = { {MSG_FSM0_VOLVO_C1, 0, 8}, {MSG_FSM1_VOLVO_C1, 0, 8},
                                     {MSG_FSM2_VOLVO_C1, 0, 8}, {MSG_FSM3_VOLVO_C1, 0, 8},
                                     {MSG_FSM4_VOLVO_C1, 0, 8},
                                     {MSG_BTNS_VOLVO_C1, 0, 8},
                                     {MSG_PSCM0_VOLVO_C1, 2, 8}, {MSG_PSCM1_VOLVO_C1, 2, 8},
                                     {MSG_DIAG_FSM, 2, 8}, {MSG_DIAG_PSCM, 0, 8},
                                     {MSG_DIAG_CEM, 0, 8}, {MSG_DIAG_CVM, 0, 8},
                                     {MSG_DIAG_BROADCAST, 0, 8}, {MSG_DIAG_BROADCAST, 2, 8},
                                    };

const int VOLVO_C1_TX_MSGS_LEN = sizeof(VOLVO_C1_TX_MSGS) / sizeof(VOLVO_C1_TX_MSGS[0]);
// platform eucd
const CanMsg VOLVO_EUCD_TX_MSGS[] = { {MSG_FSM0_VOLVO_V60, 0, 8}, {MSG_FSM1_VOLVO_V60, 0, 8},
                                       {MSG_FSM2_VOLVO_V60, 0, 8}, {MSG_FSM3_VOLVO_V60, 0, 8},
                                       {MSG_FSM4_VOLVO_V60, 0, 8}, {MSG_FSM5_VOLVO_V60, 0, 8},
                                       {MSG_PSCM1_VOLVO_V60, 2, 8},
                                       {MSG_BTNS_VOLVO_V60, 0, 8},
                                       {MSG_DIAG_FSM, 2, 8}, {MSG_DIAG_PSCM, 0, 8},
                                       {MSG_DIAG_CEM, 0, 8}, {MSG_DIAG_CVM, 0, 8},
                                       {MSG_DIAG_BROADCAST, 0, 8}, {MSG_DIAG_BROADCAST, 2, 8},
                                    };
const int VOLVO_EUCD_TX_MSGS_LEN = sizeof(VOLVO_EUCD_TX_MSGS) / sizeof(VOLVO_EUCD_TX_MSGS[0]);

// expected_timestep in microseconds between messages.
AddrCheckStruct volvo_c1_checks[] = {
  {.msg = {{MSG_FSM0_VOLVO_C1,       2, 8, .check_checksum = false, .expected_timestep = 10000U}}},
  {.msg = {{MSG_FSM1_VOLVO_C1,       2, 8, .check_checksum = false, .expected_timestep = 20000U}}},
  {.msg = {{MSG_PSCM0_VOLVO_C1,      0, 8, .check_checksum = false, .expected_timestep = 20000U}}},
  {.msg = {{MSG_PSCM1_VOLVO_C1,      0, 8, .check_checksum = false, .expected_timestep = 20000U}}},
  {.msg = {{MSG_ACC_PEDAL_VOLVO_C1,  0, 8, .check_checksum = false, .expected_timestep = 20000U}}},
};

AddrCheckStruct volvo_eucd_checks[] = {
  {.msg = {{MSG_PSCM1_VOLVO_V60,     0, 8, .check_checksum = false, .expected_timestep = 20000U}}},
  {.msg = {{MSG_FSM0_VOLVO_V60,      2, 8, .check_checksum = false, .expected_timestep = 10000U}}},
  {.msg = {{MSG_ACC_PEDAL_VOLVO_V60, 0, 8, .check_checksum = false, .expected_timestep = 10000U}}},
};

#define VOLVO_C1_RX_CHECKS_LEN sizeof(volvo_c1_checks) / sizeof(volvo_c1_checks[0])
#define VOLVO_EUCD_RX_CHECKS_LEN sizeof(volvo_eucd_checks) / sizeof(volvo_eucd_checks[0])

addr_checks volvo_c1_rx_checks = {volvo_c1_checks, VOLVO_C1_RX_CHECKS_LEN};
addr_checks volvo_eucd_rx_checks = {volvo_eucd_checks, VOLVO_EUCD_RX_CHECKS_LEN};

// Check for value in a array
/* static int val_in_arr(int val, const int arr[], const int arr_len) {
  int i;
  for(i = 0; i < arr_len; i++) {
    if(arr[i] == val) {
      return 1;
    }
  }
  return 0;
}
 */


static const addr_checks* volvo_c1_init(uint16_t param) {
  UNUSED(param);
  controls_allowed = 0;
  relay_malfunction_reset();
  giraffe_forward_camera_volvo = 0;
  return &volvo_c1_rx_checks;
}

static const addr_checks* volvo_eucd_init(uint16_t param) {
  UNUSED(param);
  controls_allowed = 0;
  relay_malfunction_reset();
  giraffe_forward_camera_volvo = 0;
  return &volvo_eucd_rx_checks;
}


static int volvo_c1_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &volvo_c1_rx_checks,
                                 NULL, NULL, NULL);

  if( valid ) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    // check acc status
    if( (addr == MSG_FSM0_VOLVO_C1) && (bus == 2) ) {
      giraffe_forward_camera_volvo = 1;
      bool acc_active = (GET_BYTE(to_push, 7) & 0x04);

      // only allow lateral control when acc active
      if(acc_active && !acc_active_prev_volvo) {
        controls_allowed = 1;
      }
      //if( !acc_active ) {
      //  controls_allowed = 0;
      //}
      acc_active_prev_volvo = acc_active;
    }

    if( bus == 0 ) {
      // Current steering angle
      if (addr == MSG_PSCM1_VOLVO_C1) {
        // 2bytes long.
        int angle_meas_new = (GET_BYTE(to_push, 5) << 8) | (GET_BYTE(to_push, 6));
        // Remove offset.
        angle_meas_new = angle_meas_new-32768;

        // update array of samples
        update_sample(&volvo_angle_meas, angle_meas_new);
      }

      // Get current speed
      if (addr == MSG_SPEED_VOLVO_C1) {
        // Factor 0.01
        volvo_speed = ((GET_BYTE(to_push, 3) << 8) | (GET_BYTE(to_push, 4))) * 0.01 / 3.6;
        //vehicle_moving = volvo_speed > 0.;
      }

      // dont forward if message is on bus 0
      if( addr == MSG_FSM0_VOLVO_C1 ) {
        giraffe_forward_camera_volvo = 0;
      }

      // If LKA msg is on bus 0, then relay is unexpectedly closed
      if( (safety_mode_cnt > RELAY_TRNS_TIMEOUT) && (addr == MSG_FSM1_VOLVO_C1) ) {
        relay_malfunction_set();
      }
    }

  }

  #ifdef DEBUG_VOLVO
  bool flag = false;
  if( controls_allowed != controls_allowed_prev_v ) {
    puts("controls_allowed:"); puth(controls_allowed); puts(" prev:"); puth(controls_allowed_prev_v); puts("\n");
    flag = true;
  }

  if( relay_malfunction != relay_malfunction_prev ) {
    puts("Relay malfunction:"); puth(relay_malfunction); puts(" prev:"); puth(relay_malfunction_prev); puts("\n");
    flag = true;
  }

  if( giraffe_forward_camera_volvo != giraffe_forward_camera_volvo_prev ) {
    puts("Giraffe forward camera volvo:"); puth(giraffe_forward_camera_volvo); puts(" prev:"); puth(relay_malfunction_prev); puts("\n");
    //puts("VOLVO V40/V60 ACC Status msg id seen on bus 0. Don't forward!\n");
    flag = true;
  }

  if( valid != valid_prev ) {
    puts("Valid:"); puth(valid); puts("\n");
    flag = true;
  }
  if( flag ) {
    puts("Loop no:"); puth(counterloop); puts("\n");
  }

  counterloop++;

  // Update old values
  relay_malfunction_prev = relay_malfunction;
  giraffe_forward_camera_volvo_prev = giraffe_forward_camera_volvo;
  controls_allowed_prev_v = controls_allowed;
  valid_prev = valid;
  #endif

  return valid;
}


static int volvo_eucd_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &volvo_eucd_rx_checks,
                                 NULL, NULL, NULL);

  if( valid ) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    // check acc status
    if( (addr == MSG_FSM0_VOLVO_V60) && (bus == 2) ) {
      giraffe_forward_camera_volvo = 1;
      int acc_status = (GET_BYTE(to_push, 2) & 0x07);
      bool acc_active = (acc_status >= 6) ? true : false;

      // only allow lateral control when acc active
      if( acc_active && !acc_active_prev_volvo ) {
        controls_allowed = 1;
      }
      if( !acc_active ) {
        controls_allowed = 0;
      }
      acc_active_prev_volvo = acc_active;
    }

    // dont forward if message is on bus 0
    if( (addr == MSG_FSM0_VOLVO_V60) && (bus == 0) ) {
      giraffe_forward_camera_volvo = 0;
    }

    // If LKA msg is on bus 0, then relay is unexpectedly closed
    if( (safety_mode_cnt > RELAY_TRNS_TIMEOUT) && (addr == MSG_FSM2_VOLVO_V60) && (bus == 0) ) {
      relay_malfunction_set();
    }
  }
  return valid;
}


static int volvo_c1_tx_hook(CANPacket_t *to_send) {

  int tx = 1;
  //int bus = GET_BUS(to_send);
  int addr = GET_ADDR(to_send);
  bool violation = 0;

  if ( !msg_allowed(to_send, VOLVO_C1_TX_MSGS, VOLVO_C1_TX_MSGS_LEN) || relay_malfunction ) {
    tx = 0;
  }

  if ( addr == MSG_FSM1_VOLVO_C1 ) {
    int desired_angle = ((GET_BYTE(to_send, 4) & 0x3f) << 8) | (GET_BYTE(to_send, 5)); // 14 bits long
    bool lka_active = (GET_BYTE(to_send, 7) & 0x3) > 0;  // Steer direction bigger than 0, commanding lka to steer

    // remove offset
    desired_angle = desired_angle-8192;

    if (controls_allowed && lka_active) {
      // add 1 to not false trigger the violation
      float delta_angle_float;
      delta_angle_float = (interpolate(VOLVO_LOOKUP_ANGLE_RATE_UP, volvo_speed) * DEG_TO_CAN_VOLVO_C1) + 1.;
      int delta_angle_up = (int)(delta_angle_float);
      delta_angle_float =  (interpolate(VOLVO_LOOKUP_ANGLE_RATE_DOWN, volvo_speed) * DEG_TO_CAN_VOLVO_C1) + 1.;
      int delta_angle_down = (int)(delta_angle_float);
      int highest_desired_angle = volvo_desired_angle_last + ((volvo_desired_angle_last > 0) ? delta_angle_up : delta_angle_down);
      int lowest_desired_angle = volvo_desired_angle_last - ((volvo_desired_angle_last >= 0) ? delta_angle_down : delta_angle_up);

      // max request offset from actual angle
      int hi_angle_req = MIN(desired_angle + VOLVO_MAX_DELTA_OFFSET_ANGLE, VOLVO_MAX_ANGLE_REQ);
      int lo_angle_req = MAX(desired_angle - VOLVO_MAX_DELTA_OFFSET_ANGLE, VOLVO_MIN_ANGLE_REQ);

      // check for violation;
      violation |= max_limit_check(desired_angle, highest_desired_angle, lowest_desired_angle);
      violation |= max_limit_check(desired_angle, hi_angle_req, lo_angle_req);
    }
    volvo_desired_angle_last = desired_angle;

    // desired steer angle should be the same as steer angle measured when controls are off
    // dont check when outside of measurable range. desired_angle can only be -8192->8191 (+-360).
    if ((!controls_allowed)
          && ((volvo_angle_meas.min - 1) >= VOLVO_MAX_ANGLE_REQ)
          && ((volvo_angle_meas.max + 1) <= VOLVO_MIN_ANGLE_REQ)
          && ((desired_angle < (volvo_angle_meas.min - 1)) || (desired_angle > (volvo_angle_meas.max + 1)))) {
      violation = 1;
    }

    // no lka_enabled bit if controls not allowed
    if (!controls_allowed && lka_active) {
      violation = 1;
    }
  }

  // acc button check, only allow cancel button to be sent
  //if (addr == MSG_BTNS_VOLVO_C1) {
    // Violation if any button other than cancel is pressed
  //  violation |= ((GET_BYTE(to_send, 7) & 0xef) > 0) | (GET_BYTE(to_send, 6) > 0);
  //}

  if (violation) {
    controls_allowed = 0;
    tx = 0;
  }

  return tx;
}


static int volvo_eucd_tx_hook(CANPacket_t *to_send) {

  //int bus = GET_BUS(to_send);
  //int addr = GET_ADDR(to_send);

  int tx = 1;

  if ( !msg_allowed(to_send, VOLVO_EUCD_TX_MSGS, VOLVO_EUCD_TX_MSGS_LEN) || relay_malfunction ) {
    tx = 0;
  }

  return tx;
}


static int volvo_c1_fwd_hook(int bus_num, CANPacket_t *to_fwd) {

  int bus_fwd = -1; // fallback to do not forward
  int addr = GET_ADDR(to_fwd);

  if( !relay_malfunction && giraffe_forward_camera_volvo ) {
    if( bus_num == 0 ){
      bool block_msg = (addr == MSG_PSCM1_VOLVO_C1);
      if ( !block_msg ) {
        bus_fwd = 2; // forward 0 -> 2
      }
      //val_in_arr(addr, ALLOWED_MSG_C1, ALLOWED_MSG_C1_LEN); // block not relevant msgs
      //bool allw_msg = val_in_arr(addr, ALLOWED_MSG_C1, ALLOWED_MSG_C1_LEN); // block not relevant msgs
      //bus_fwd = allw_msg ? 2 : -1;  // forward bus 0 -> 2
    }

    if( bus_num == 2 ) {
      bool block_msg = (addr == MSG_FSM1_VOLVO_C1); // block if lkas msg
      if( !block_msg ) {
        bus_fwd = 0; // forward bus 2 -> 0
      }
    }
  }
  return bus_fwd;
}


static int volvo_eucd_fwd_hook(int bus_num, CANPacket_t *to_fwd) {

  int bus_fwd = -1; // fallback to do not forward
  int addr = GET_ADDR(to_fwd);

  if( !relay_malfunction && giraffe_forward_camera_volvo ) {
    if( bus_num == 0 ){
      bool block_msg = (addr == MSG_PSCM1_VOLVO_V60);
      //bool allw_msg = val_in_arr(addr, ALLOWED_MSG_EUCD, ALLOWED_MSG_EUCD_LEN); // block not relevant msgs
      bus_fwd = block_msg ? -1 : 2;  // forward bus 0 -> 2
    }

    if( bus_num == 2 ) {
      bool block_msg = (addr == MSG_FSM2_VOLVO_V60);  // block if lkas msg
      if( !block_msg ) {
        bus_fwd = 0; // forward bus 2 -> 0
      }
    }
  }
  return bus_fwd;
}


const safety_hooks volvo_c1_hooks = {
  .init = volvo_c1_init,
  .rx = volvo_c1_rx_hook,
  .tx = volvo_c1_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volvo_c1_fwd_hook,
};


const safety_hooks volvo_eucd_hooks = {
  .init = volvo_eucd_init,
  .rx = volvo_eucd_rx_hook,
  .tx = volvo_eucd_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volvo_eucd_fwd_hook,
};
