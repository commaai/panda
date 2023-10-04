/*
Volvo Electronic Control Units abbreviations and network topology
Platforms C1

Look in selfdrive/car/volvo/values.py for more information.
*/

// Globals
// msg ids
#define MSG_BTNS_VOLVO_C1 0x10      // Steering wheel buttons
#define MSG_FSM0_VOLVO_C1 0x30      // ACC status message
#define MSG_FSM1_VOLVO_C1 0xd0      // LKA steering message
#define MSG_PSCM1_VOLVO_C1 0x125    // Steering angle from servo
#define MSG_ACC_PEDAL_VOLVO_C1 0x55 // Gas pedal
#define MSG_SPEED_VOLVO_C1 0x130    // Speed signal

// safety params
const SteeringLimits VOLVO_STEERING_LIMITS = {
  .enforce_angle_error = true,
  .inactive_angle_is_zero = true,
  .angle_deg_to_can = 1/.04395,  // 22.753... inverse of dbc scaling 
  .angle_rate_up_lookup = {
    {7., 17., 36.},
    {2, .25, .1}
  },
  .angle_rate_down_lookup = {
    {7., 17., 36.},
    {2, .25, .1}
  },
};

// TX checks
// platform c1
const CanMsg VOLVO_TX_MSGS[] = { {MSG_FSM1_VOLVO_C1, 0, 8},
                                 {MSG_BTNS_VOLVO_C1, 0, 8},
                                 {MSG_PSCM1_VOLVO_C1, 2, 8},
};

const int VOLVO_TX_MSGS_LEN = sizeof(VOLVO_TX_MSGS) / sizeof(VOLVO_TX_MSGS[0]);

// expected_timestep in microseconds between messages.
// WD timeout in external black panda if monitoring all messages.
// However works with built-in panda in Comma 3.
AddrCheckStruct volvo_checks[] = {
  //{.msg = {{MSG_FSM0_VOLVO_C1,       2, 8, .check_checksum = false, .expected_timestep = 10000U}, {0}, {0}}},
  {.msg = {{MSG_FSM1_VOLVO_C1,       2, 8, .check_checksum = false, .expected_timestep = 20000U}, {0}, {0}}},
  {.msg = {{MSG_PSCM1_VOLVO_C1,      0, 8, .check_checksum = false, .expected_timestep = 20000U}, {0}, {0}}},
  {.msg = {{MSG_ACC_PEDAL_VOLVO_C1,  0, 8, .check_checksum = false, .expected_timestep = 20000U}, {0}, {0}}},
};

#define VOLVO_RX_CHECKS_LEN sizeof(volvo_checks) / sizeof(volvo_checks[0])
addr_checks volvo_rx_checks = {volvo_checks, VOLVO_RX_CHECKS_LEN};

static const addr_checks* volvo_init(uint16_t param) {
  UNUSED(param);
  return &volvo_rx_checks;
}

static int volvo_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &volvo_rx_checks,
                                 NULL, NULL, NULL, NULL);

  if( valid ) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    // check acc status
    if( (addr == MSG_FSM0_VOLVO_C1) && (bus == 2) ) {
      bool acc_active = (GET_BYTE(to_push, 7) & 0x04);
      pcm_cruise_check(acc_active);
    }

    if( bus == 0 ) {
      if (addr == MSG_PSCM1_VOLVO_C1) {
        // Current steering angle
        // 2bytes long.
        int angle_meas_new = (GET_BYTE(to_push, 5) << 8) | (GET_BYTE(to_push, 6));
        // Remove offset.
        angle_meas_new = angle_meas_new-32768;

        // update array of samples
        update_sample(&angle_meas, angle_meas_new);
      }

      // Get current speed
      if (addr == MSG_SPEED_VOLVO_C1) {
        // Factor 0.01 to km/h. division by 3.6 -> m/s.
        int volvo_speed = GET_BYTE(to_push, 3) << 8 | GET_BYTE(to_push, 4);
        volvo_speed = ROUND( (volvo_speed * 0.01 / 3.6) * VEHICLE_SPEED_FACTOR );
        update_sample(&vehicle_speed, volvo_speed);
        vehicle_moving = volvo_speed > 0.;
      }

      generic_rx_checks((addr == MSG_FSM1_VOLVO_C1) && (bus == 0));
    }
  }

  return valid;
}


static int volvo_tx_hook(CANPacket_t *to_send) {

  int tx = 1;
  int addr = GET_ADDR(to_send);

  if ( !msg_allowed(to_send, VOLVO_TX_MSGS, VOLVO_TX_MSGS_LEN) ) {
    tx = 0;
  }

  if ( addr == MSG_FSM1_VOLVO_C1 ) {
    int desired_angle = ((GET_BYTE(to_send, 4) & 0x3f) << 8) | (GET_BYTE(to_send, 5)); // 14 bits long
    bool lka_active = (GET_BYTE(to_send, 7) & 0x3) > 0;  // Steer direction bigger than 0 -> commanding lka to steer

    // remove offset from desired angle
    // same factor as PSCM1 message but another offset.
    desired_angle = desired_angle-8192;
    
    // WD TIMEOUT in external black panda, works with built-in panda in Comma 3.
    if (steer_angle_cmd_checks(desired_angle, lka_active, VOLVO_STEERING_LIMITS)) {
      tx = 0;
    }
    
    // no lka_enabled bit if controls not allowed
    if (!controls_allowed && lka_active) {
      tx = 0;
    }
    
  }

  // acc button check, only allow cancel button to be sent
  if (addr == MSG_BTNS_VOLVO_C1) {
    // Violation if any button other than cancel is pressed
    if( (GET_BYTE(to_send, 7) & 0xef) | GET_BYTE(to_send, 6) ) {
      tx = 0;
    }
  }

  return tx;
}

static int volvo_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1; // fallback to do not forward

  if( bus_num == 0 ){
    bool block_msg = (addr == MSG_PSCM1_VOLVO_C1);
    if ( !block_msg ) {
      bus_fwd = 2; // forward 0 -> 2
    }
  }

  if( bus_num == 2 ) {
    bool block_msg = (addr == MSG_FSM1_VOLVO_C1); // block if lkas msg
    if( !block_msg ) {
      bus_fwd = 0; // forward bus 2 -> 0
    }
  }
  return bus_fwd;
}

const safety_hooks volvo_hooks = {
  .init = volvo_init,
  .rx = volvo_rx_hook,
  .tx = volvo_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volvo_fwd_hook,
};
