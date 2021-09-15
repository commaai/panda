// TODO: haven't validated these numbers, also need to comply with comma safety standards
const int STELLANTIS_MAX_STEER = 255;               // TODO: verify
const int STELLANTIS_MAX_RT_DELTA = 93;             // 5 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 62 ; 62 * 1.5 for safety pad = 93
const uint32_t STELLANTIS_RT_INTERVAL = 250000;     // 250ms between real time checks
const int STELLANTIS_MAX_RATE_UP = 5;               // observed up to 5 up from factory LKAS
const int STELLANTIS_MAX_RATE_DOWN = 10;            // observed up to 50 down from factory LKAS
const int STELLANTIS_DRIVER_TORQUE_ALLOWANCE = 80;  // TODO: verify suitability
const int STELLANTIS_DRIVER_TORQUE_FACTOR = 3;      // TODO: verify suitability

// Safety-relevant CAN messages for the Stellantis 5th gen RAM (DT) platform
#define MSG_EPS_2           0x31  // EPS driver input torque and angle-change rate
#define MSG_ABS_1           0x79  // Brake pedal and pressure
#define MSG_TPS_1           0x81  // Throttle position sensor
#define MSG_ABS_4           0x8B  // ABS wheel speeds
#define MSG_DASM_ACC_CMD_1  0x99  // ACC engagement states from DASM
#define MSG_DASM_LKAS_CMD   0xA6  // LKAS controls from DASM
#define MSG_ACC_BUTTONS     0xB1  // Cruise control buttons
#define MSG_DASM_LKAS_HUD   0xFA  // LKAS HUD and auto headlight control from DASM

const CanMsg STELLANTIS_TX_MSGS[] = {{MSG_DASM_LKAS_CMD, 0, 8}, {MSG_DASM_LKAS_HUD, 0, 8}, {MSG_ACC_BUTTONS, 2, 3}};

AddrCheckStruct stellantis_addr_checks[] = {
  {.msg = {{MSG_EPS_2, 0, 8, .check_checksum = true, .max_counter = 15U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ABS_1, 0, 8, .check_checksum = true, .max_counter = 15U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_TPS_1, 0, 8, .check_checksum = false, .max_counter = 0U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ABS_4, 0, 8, .check_checksum = false, .max_counter = 0U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_DASM_ACC_CMD_1, 2, 8, .check_checksum = true, .max_counter = 15U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define STELLANTIS_ADDR_CHECK_LEN (sizeof(stellantis_addr_checks) / sizeof(stellantis_addr_checks[0]))
addr_checks stellantis_rx_checks = {stellantis_addr_checks, STELLANTIS_ADDR_CHECK_LEN};

static uint8_t stellantis_get_checksum(CAN_FIFOMailBox_TypeDef *to_push) {
  int checksum_byte = GET_LEN(to_push) - 1;
  return (uint8_t)(GET_BYTE(to_push, checksum_byte));
}

// TODO: centralize/combine with Chrysler?
static uint8_t stellantis_compute_checksum(CAN_FIFOMailBox_TypeDef *to_push) {
  /* This function does not want the checksum byte in the input data.
  jeep ram canbus checksum from http://illmatics.com/Remote%20Car%20Hacking.pdf */
  uint8_t checksum = 0xFFU;
  int len = GET_LEN(to_push);
  for (int j = 0; j < (len - 1); j++) {
    uint8_t shift = 0x80U;
    uint8_t curr = (uint8_t)GET_BYTE(to_push, j);
    for (int i=0; i<8; i++) {
      uint8_t bit_sum = curr & shift;
      uint8_t temp_chk = checksum & 0x80U;
      if (bit_sum != 0U) {
        bit_sum = 0x1C;
        if (temp_chk != 0U) {
          bit_sum = 1;
        }
        checksum = checksum << 1;
        temp_chk = checksum | 1U;
        bit_sum ^= temp_chk;
      } else {
        if (temp_chk != 0U) {
          bit_sum = 0x1D;
        }
        checksum = checksum << 1;
        bit_sum ^= checksum;
      }
      checksum = bit_sum;
      shift = shift >> 1;
    }
  }
  return ~checksum;
}

static uint8_t stellantis_get_counter(CAN_FIFOMailBox_TypeDef *to_push) {
  // Well defined counter only for 8 bytes messages
  return (uint8_t)(GET_BYTE(to_push, 6) >> 4);
}

static int stellantis_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  bool valid = addr_safety_check(to_push, &stellantis_rx_checks,
                                 stellantis_get_checksum, stellantis_compute_checksum, stellantis_get_counter);

  if (valid) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    // Measured eps torque
    if ((bus == 0) && (addr == MSG_EPS_2)) {
      int torque_driver_new = (((GET_BYTE(to_push, 0) & 0xFU) << 8) | GET_BYTE(to_push, 1));
      torque_driver_new = to_signed(torque_driver_new, 12) + 1024;

      // update array of samples
      update_sample(&torque_driver, torque_driver_new);
    }

    // enter controls on rising edge of ACC, exit controls on ACC off
    if ((bus == 2) && (addr == MSG_DASM_ACC_CMD_1)) {
      int cruise_engaged = ((GET_BYTE(to_push, 2) >> 4) & 0x3U) == 0x3U;
      if (cruise_engaged && !cruise_engaged_prev) {
        controls_allowed = 1;
      }
      if (!cruise_engaged) {
        controls_allowed = 0;
      }
      cruise_engaged_prev = cruise_engaged;
    }

    // update speed
    if ((bus == 0) && (addr == MSG_ABS_4)) {
      int wheel_speed_fl = ((GET_BYTE(to_push, 4) & 0xFU) << 8) | GET_BYTE(to_push, 5);
      int wheel_speed_fr = ((GET_BYTE(to_push, 6) & 0xFU) << 8) | GET_BYTE(to_push, 7);
      // Check for average front speed in excess of 0.3m/s, 1.08km/h
      // DBC speed scale 0.02: 0.3m/s = 15, sum both wheels to compare
      vehicle_moving = (wheel_speed_fl + wheel_speed_fr) > 30;
    }

    // exit controls on rising edge of gas press
    if ((bus == 0) && (addr == MSG_TPS_1)) {
      gas_pressed = GET_BYTE(to_push, 5) > 20;  // FIXME: this signal is suspect, nonzero on *some* vehicles/drives
    }

    // exit controls on rising edge of brake press
    if ((bus == 0) && (addr == MSG_ABS_1)) {
      brake_pressed = (((GET_BYTE(to_push, 2) & 0xFU) << 8) | GET_BYTE(to_push, 3)) > 0;
      if (brake_pressed && (!brake_pressed_prev || vehicle_moving)) {
        controls_allowed = 0;
      }
      brake_pressed_prev = brake_pressed;
    }

    if (bus == 0) {
      generic_rx_checks((addr == MSG_DASM_LKAS_CMD));
    }
  }
  return valid;
}

static int stellantis_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  int tx = 1;
  int addr = GET_ADDR(to_send);

  if (!msg_allowed(to_send, STELLANTIS_TX_MSGS, sizeof(STELLANTIS_TX_MSGS) / sizeof(STELLANTIS_TX_MSGS[0]))) {
    tx = 0;
  }

  if (relay_malfunction) {
    tx = 0;
  }

  // LKA STEER
  if (addr == MSG_DASM_LKAS_CMD) {
    int desired_torque = ((GET_BYTE(to_send, 1) << 8) | GET_BYTE(to_send, 2)) - 1024U;
    uint32_t ts = microsecond_timer_get();
    bool violation = 0;

    if (controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, STELLANTIS_MAX_STEER, -STELLANTIS_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, desired_torque_last, &torque_driver,
        STELLANTIS_MAX_STEER, STELLANTIS_MAX_RATE_UP, STELLANTIS_MAX_RATE_DOWN,
        STELLANTIS_DRIVER_TORQUE_ALLOWANCE, STELLANTIS_DRIVER_TORQUE_FACTOR);

      // used next time
      desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, rt_torque_last, STELLANTIS_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
      if (ts_elapsed > STELLANTIS_RT_INTERVAL) {
        rt_torque_last = desired_torque;
        ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = 1;
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

  // FORCE CANCEL: only the cancel button press is allowed
  if ((addr == MSG_ACC_BUTTONS) && !controls_allowed) {
    if ((GET_BYTE(to_send, 0) & 0x14) != 0) {
      tx = 0;
    }
  }

  return tx;
}

static int stellantis_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {

  int bus_fwd = -1;
  int addr = GET_ADDR(to_fwd);

  if (!relay_malfunction) {
    // forward all messages from the rest of the car toward DASM
    if (bus_num == 0) {
      bus_fwd = 2;
    }
    // selectively forward messages from DASM toward the rest of the car
    if ((bus_num == 2) && (addr != MSG_DASM_LKAS_CMD) && (addr != MSG_DASM_LKAS_HUD)) {
      bus_fwd = 0;
    }
  }
  return bus_fwd;
}

static const addr_checks* stellantis_init(int16_t param) {
  UNUSED(param);  // TODO: may need a parameter to choose steering angle signal width/precision for 2019 vs 2021 RAM
  controls_allowed = false;
  relay_malfunction_reset();
  return &stellantis_rx_checks;
}

const safety_hooks stellantis_hooks = {
  .init = stellantis_init,
  .rx = stellantis_rx_hook,
  .tx = stellantis_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = stellantis_fwd_hook,
};
