// lateral limits
const SteeringLimits HONGQI_STEERING_LIMITS = {
  .max_steer = 300,              // As-yet unknown fault boundary, guessing 300 / 3.0Nm for now
  .max_rt_delta = 113,           // 6 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 75 ; 50 * 1.5 for safety pad = 113
  .max_rt_interval = 250000,     // 250ms between real time checks
  .max_rate_up = 6,              // 10 unit/sec observed from factory LKAS, fault boundary unknown
  .max_rate_down = 10,           // 10 unit/sec observed from factory LKAS, fault boundary unknown
  .driver_torque_allowance = 50,
  .driver_torque_factor = 3,
  .type = TorqueDriverLimited,
};

#define MSG_ECM_1           0x92    // RX from ABS, for brake pressures
#define MSG_ABS_1           0xC0    // RX from ABS, for wheel speeds
#define MSG_MAYBE_ABS       0x94    // RX from ABS? has brake-pressed and other signals
#define MSG_ACC             0x110   // RX from ACC, for ACC engagement state
#define MSG_LKAS            0x112   // TX from openpilot, for LKAS torque
#define MSG_EPS_2           0x150   // RX from EPS, torque inputs and outputs

const CanMsg HONGQI_TX_MSGS[] = {{MSG_LKAS, 0, 8}};
#define HONGQI_TX_MSGS_LEN (sizeof(HONGQI_TX_MSGS) / sizeof(HONGQI_TX_MSGS[0]))

AddrCheckStruct hongqi_addr_checks[] = {
  {.msg = {{MSG_ECM_1, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ABS_1, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MAYBE_ABS, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ACC, 2, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_EPS_2, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define HONGQI_ADDR_CHECKS_LEN (sizeof(hongqi_addr_checks) / sizeof(hongqi_addr_checks[0]))
addr_checks hongqi_rx_checks = {hongqi_addr_checks, HONGQI_ADDR_CHECKS_LEN};


static uint32_t hongqi_get_checksum(CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t hongqi_get_counter(CANPacket_t *to_push) {
  return ((uint8_t)GET_BYTE(to_push, 7) >> 4) & 0xFU;
}

static uint32_t hongqi_compute_checksum(CANPacket_t *to_push) {
  int len = GET_LEN(to_push);
  int checksum = 0;

  for(int i = 1; i < len; i++) {
    checksum ^= (uint8_t)GET_BYTE(to_push, i);
  }

  return checksum;
}

static const addr_checks* hongqi_init(uint16_t param) {
  UNUSED(param);

  return &hongqi_rx_checks;
}

static int hongqi_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &hongqi_rx_checks, hongqi_get_checksum, hongqi_compute_checksum, hongqi_get_counter);
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if (valid && (bus == 2)) {
    // Enter controls on rising edge of stock ACC, exit controls if stock ACC disengages
    // Signal: ACC.STATUS
    if (addr == MSG_ACC) {
      int acc_status = (GET_BYTE(to_push, 4) & 0xF0U) >> 4;
      bool cruise_engaged = ((acc_status == 4) || (acc_status == 5) || (acc_status == 6) || (acc_status == 7));
      pcm_cruise_check(cruise_engaged);
    }
  }

  if (valid && (bus == 0)) {
    // Update in-motion state by sampling front wheel speeds
    // Signal: ABS_1.FRONT_LEFT in scaled km/h
    // Signal: ABS_1.FRONT_RIGHT in scaled km/h
    if (addr == MSG_ABS_1) {
      int wheel_speed_fl = GET_BYTE(to_push, 1) | (GET_BYTE(to_push, 2) << 8);
      int wheel_speed_fr = GET_BYTE(to_push, 3) | (GET_BYTE(to_push, 4) << 8);
      // Check for average front speed in excess of 0.3m/s, 1.08km/h
      // DBC speed scale 0.01: 0.3m/s = 108, sum both wheels to compare
      vehicle_moving = (wheel_speed_fl + wheel_speed_fr) > 216;
    }

    // Update driver input torque samples
    // Signal: EPS_2.DRIVER_INPUT_TORQUE (absolute torque)
    // Signal: EPS_2.EPS_TORQUE_DIRECTION (direction) (FIXME: may not be the correct direction signal)
    if (addr == MSG_EPS_2) {
      int torque_driver_new = GET_BYTE(to_push, 4);
      int sign = GET_BIT(to_push, 18U);
      if (sign == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&torque_driver, torque_driver_new);
    }

    // Signal: ECM_1.DRIVER_THROTTLE
    if (addr == MSG_ECM_1) {
      gas_pressed = (GET_BYTE(to_push, 5) != 0U);
    }

    // Signal: MAYBE_ABS.BRAKE_PRESSED
    if (addr == MSG_MAYBE_ABS) {
      brake_pressed = GET_BIT(to_push, 35U);
    }

    generic_rx_checks((addr == MSG_LKAS));
  }

  return valid;
}

static int hongqi_tx_hook(CANPacket_t *to_send) {
  int addr = GET_ADDR(to_send);
  int tx = 1;

  if (!msg_allowed(to_send, HONGQI_TX_MSGS, HONGQI_TX_MSGS_LEN)) {
    tx = 0;
  }

  // Safety check for LKAS torque
  // Signal: LKAS.LKAS_TORQUE
  // Signal: LKAS.LKAS_TORQUE_DIRECTION
  if (addr == MSG_LKAS) {
    int desired_torque = GET_BYTE(to_send, 1) | ((GET_BYTE(to_send, 2) & 0x3U) << 8);
    int sign = (GET_BYTE(to_send, 2) & 0x4U) >> 2;
    // Hongqi sends 1022 when steering is inactive
    if (desired_torque == 1022) {
      desired_torque = 0;
    }
    if (sign == 1) {
      desired_torque *= -1;
    }

    if (steer_torque_cmd_checks(desired_torque, -1, HONGQI_STEERING_LIMITS)) {
      tx = 0;
    }
  }

  // FORCE CANCEL: ensuring that only the cancel button press is sent when controls are off.
  // This avoids unintended engagements while still allowing resume spam
  // TODO: implement this
  //if ((addr == MSG_GRA_ACC_01) && !controls_allowed) {
  //  // disallow resume and set: bits 16 and 19
  //  if ((GET_BYTE(to_send, 2) & 0x9U) != 0U) {
  //    tx = 0;
  //  }
  //}

  // 1 allows the message through
  return tx;
}

static int hongqi_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  switch (bus_num) {
    case 0:
      bus_fwd = 2;
      break;
    case 2:
      if (addr == MSG_LKAS) {
        // OP takes control of the LKAS messages from the camera
        bus_fwd = -1;
      } else {
        bus_fwd = 0;
      }
      break;
    default:
      // No other buses should be in use; fallback to do-not-forward
      bus_fwd = -1;
      break;
  }

  return bus_fwd;
}

const safety_hooks hongqi_hooks = {
  .init = hongqi_init,
  .rx = hongqi_rx_hook,
  .tx = hongqi_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = hongqi_fwd_hook,
};
