const int FAW_MAX_STEER = 300;               // As-yet unknown fault boundary, guessing 300 / 3.0Nm for now
const int FAW_MAX_RT_DELTA = 56;             // 3 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 37.5 ; 50 * 1.5 for safety pad = 56.25
const uint32_t FAW_RT_INTERVAL = 250000;     // 250ms between real time checks
const int FAW_MAX_RATE_UP = 3;               // 3 unit/sec observed from factory LKAS, fault boundary unknown
const int FAW_MAX_RATE_DOWN = 3;             // 3 unit/sec observed from factory LKAS, fault boundary unknown
const int FAW_DRIVER_TORQUE_ALLOWANCE = 25;
const int FAW_DRIVER_TORQUE_FACTOR = 3;

#define MSG_ECM_1           0x92    // RX from ABS, for brake pressures
#define MSG_ABS_1           0xC0    // RX from ABS, for wheel speeds
#define MSG_ABS_2           0xC2    // RX from ABS, for wheel speeds and braking
#define MSG_ACC             0x110   // RX from ACC, for ACC engagement state
#define MSG_LKAS            0x112   // TX from openpilot, for LKAS torque
#define MSG_EPS_2           0x150   // RX from EPS, torque inputs and outputs

// Transmit of GRA_ACC_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const CanMsg FAW_TX_MSGS[] = {{MSG_LKAS, 0, 8}};
#define FAW_TX_MSGS_LEN (sizeof(FAW_TX_MSGS) / sizeof(FAW_TX_MSGS[0]))

AddrCheckStruct faw_addr_checks[] = {
  {.msg = {{MSG_ECM_1, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ABS_1, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ABS_2, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ACC, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_EPS_2, 0, 8, .check_checksum = true, .max_counter = 15U,  .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define FAW_ADDR_CHECKS_LEN (sizeof(faw_addr_checks) / sizeof(faw_addr_checks[0]))
addr_checks faw_rx_checks = {faw_addr_checks, FAW_ADDR_CHECKS_LEN};


static uint8_t faw_get_checksum(CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t faw_get_counter(CANPacket_t *to_push) {
  return ((uint8_t)GET_BYTE(to_push, 7) >> 4) & 0xFU;
}

static uint8_t faw_compute_checksum(CANPacket_t *to_push) {
  int len = GET_LEN(to_push);
  int checksum = 0;

  for(int i = 1; i < len; i++) {
    checksum ^= (uint8_t)GET_BYTE(to_push, i);
  }

  return checksum;
}

static const addr_checks* faw_init(int16_t param) {
  UNUSED(param);

  controls_allowed = false;
  relay_malfunction_reset();
  return &faw_rx_checks;
}

static int faw_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &faw_rx_checks, faw_get_checksum, faw_compute_checksum, faw_get_counter);

  if (valid && (GET_BUS(to_push) == 0U)) {
    int addr = GET_ADDR(to_push);

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
      int sign = (GET_BYTE(to_push, 2) & 0x4U) >> 2;
      if (sign == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&torque_driver, torque_driver_new);
    }

    // Enter controls on rising edge of stock ACC, exit controls if stock ACC disengages
    // Signal: ACC.STATUS
    if (addr == MSG_ACC) {
      int acc_status = (GET_BYTE(to_push, 4) & 0x7CU) >> 2;
      int cruise_engaged = acc_status == 20;
      if (cruise_engaged && !cruise_engaged_prev) {
        controls_allowed = 1;
      }
      if (!cruise_engaged) {
        controls_allowed = 0;
      }
      cruise_engaged_prev = cruise_engaged;
    }

    // Signal: ECM_1.DRIVER_THROTTLE
    if (addr == MSG_ECM_1) {
      gas_pressed = GET_BYTE(to_push, 5) != 0;
    }

    // Signal: ABS_2.BRAKE_PRESSURE
    if (addr == MSG_ABS_2) {
      brake_pressed = GET_BYTE(to_push, 5) != 0;
    }

    generic_rx_checks((addr == MSG_LKAS));
  }
  return valid;
}

static int faw_tx_hook(CANPacket_t *to_send) {
  int addr = GET_ADDR(to_send);
  int tx = 1;

  if (!msg_allowed(to_send, FAW_TX_MSGS, FAW_TX_MSGS_LEN)) {
    tx = 0;
  }

  // Safety check for LKAS torque
  // Signal: LKAS.LKAS_TORQUE
  // Signal: LKAS.LKAS_TORQUE_DIRECTION
  if (addr == MSG_LKAS) {
    int desired_torque = GET_BYTE(to_send, 1) | ((GET_BYTE(to_send, 2) & 0x3U) << 8);
    int sign = (GET_BYTE(to_send, 2) & 0x4U) >> 2;
    if (sign == 1) {
      desired_torque *= -1;
    }

    bool violation = false;
    uint32_t ts = microsecond_timer_get();

    if (controls_allowed) {
      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, FAW_MAX_STEER, -FAW_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, desired_torque_last, &torque_driver,
        FAW_MAX_STEER, FAW_MAX_RATE_UP, FAW_MAX_RATE_DOWN,
        FAW_DRIVER_TORQUE_ALLOWANCE, FAW_DRIVER_TORQUE_FACTOR);
      desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, rt_torque_last, FAW_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
      if (ts_elapsed > FAW_RT_INTERVAL) {
        rt_torque_last = desired_torque;
        ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = true;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      desired_torque_last = 0;
      rt_torque_last = 0;
      ts_last = ts;
    }

    if (violation) {
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

static int faw_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
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

const safety_hooks faw_hooks = {
  .init = faw_init,
  .rx = faw_rx_hook,
  .tx = faw_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = faw_fwd_hook,
};
