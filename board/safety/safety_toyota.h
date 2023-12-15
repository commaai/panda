const SteeringLimits TOYOTA_STEERING_LIMITS = {
  .max_steer = 1500,
  .max_rate_up = 15,          // ramp up slow
  .max_rate_down = 25,        // ramp down fast
  .max_torque_error = 350,    // max torque cmd in excess of motor torque
  .max_rt_delta = 450,        // the real time limit is 1800/sec, a 20% buffer
  .max_rt_interval = 250000,
  .type = TorqueMotorLimited,

  // the EPS faults when the steering angle rate is above a certain threshold for too long. to prevent this,
  // we allow setting STEER_REQUEST bit to 0 while maintaining the requested torque value for a single frame
  .min_valid_request_frames = 18,
  .max_invalid_request_frames = 1,
  .min_valid_request_rt_interval = 170000,  // 170ms; a ~10% buffer on cutting every 19 frames
  .has_steer_req_tolerance = true,

  // LTA angle limits
  // factor for STEER_TORQUE_SENSOR->STEER_ANGLE and STEERING_LTA->STEER_ANGLE_CMD (1 / 0.0573)
  .angle_deg_to_can = 17.452007,
  .angle_rate_up_lookup = {
    {5., 25., 25.},
    {0.3, 0.15, 0.15}
  },
  .angle_rate_down_lookup = {
    {5., 25., 25.},
    {0.36, 0.26, 0.26}
  },
};

const int TOYOTA_LTA_MAX_ANGLE = 1657;  // EPS only accepts up to 94.9461
const int TOYOTA_LTA_MAX_MEAS_TORQUE = 1500;
const int TOYOTA_LTA_MAX_DRIVER_TORQUE = 150;

// longitudinal limits
const LongitudinalLimits TOYOTA_LONG_LIMITS = {
  .max_accel = 2000,   // 2.0 m/s2
  .min_accel = -3500,  // -3.5 m/s2
};

// panda interceptor threshold needs to be equivalent to openpilot threshold to avoid controls mismatches
// If thresholds are mismatched then it is possible for panda to see the gas fall and rise while openpilot is in the pre-enabled state
// Threshold calculated from DBC gains: round((((15 + 75.555) / 0.159375) + ((15 + 151.111) / 0.159375)) / 2) = 805
const int TOYOTA_GAS_INTERCEPTOR_THRSLD = 805;
#define TOYOTA_GET_INTERCEPTOR(msg) (((GET_BYTE((msg), 0) << 8) + GET_BYTE((msg), 1) + (GET_BYTE((msg), 2) << 8) + GET_BYTE((msg), 3)) / 2U) // avg between 2 tracks

#define TOYOTA_COMMON_TX_MSGS                                                                                                               \
  {0x283, 0, 7}, {0x2E6, 0, 8}, {0x2E7, 0, 8}, {0x33E, 0, 7}, {0x344, 0, 8}, {0x365, 0, 7}, {0x366, 0, 7}, {0x4CB, 0, 8},  /* DSU bus 0 */  \
  {0x128, 1, 6}, {0x141, 1, 4}, {0x160, 1, 8}, {0x161, 1, 7}, {0x470, 1, 4},  /* DSU bus 1 */                                               \
  {0x2E4, 0, 5}, {0x191, 0, 8}, {0x411, 0, 8}, {0x412, 0, 8}, {0x343, 0, 8}, {0x1D2, 0, 8},  /* LKAS + ACC */                               \

const CanMsg TOYOTA_TX_MSGS[] = {
  TOYOTA_COMMON_TX_MSGS
};

const CanMsg TOYOTA_INTERCEPTOR_TX_MSGS[] = {
  TOYOTA_COMMON_TX_MSGS
  {0x200, 0, 6},  // gas interceptor
};

#define TOYOTA_COMMON_RX_CHECKS(lta)                                                                        \
  {.msg = {{ 0xaa, 0, 8, .check_checksum = false, .frequency = 83U}, { 0 }, { 0 }}},                        \
  {.msg = {{0x260, 0, 8, .check_checksum = true, .quality_flag = (lta), .frequency = 50U}, { 0 }, { 0 }}},  \
  {.msg = {{0x1D2, 0, 8, .check_checksum = true, .frequency = 33U}, { 0 }, { 0 }}},                         \
  {.msg = {{0x224, 0, 8, .check_checksum = false, .frequency = 40U},                                        \
           {0x226, 0, 8, .check_checksum = false, .frequency = 40U}, { 0 }}},                               \

RxCheck toyota_lka_rx_checks[] = {
  TOYOTA_COMMON_RX_CHECKS(false)
};

RxCheck toyota_lka_interceptor_rx_checks[] = {
  TOYOTA_COMMON_RX_CHECKS(false)
  {.msg = {{0x201, 0, 6, .check_checksum = false, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},
};

// Check the quality flag for angle measurement when using LTA, since it's not set on TSS-P cars
RxCheck toyota_lta_rx_checks[] = {
  TOYOTA_COMMON_RX_CHECKS(true)
};

RxCheck toyota_lta_interceptor_rx_checks[] = {
  TOYOTA_COMMON_RX_CHECKS(true)
  {.msg = {{0x201, 0, 6, .check_checksum = false, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},
};

// safety param flags
// first byte is for EPS factor, second is for flags
const uint32_t TOYOTA_PARAM_OFFSET = 8U;
const uint32_t TOYOTA_EPS_FACTOR = (1U << TOYOTA_PARAM_OFFSET) - 1U;
const uint32_t TOYOTA_PARAM_ALT_BRAKE = 1U << TOYOTA_PARAM_OFFSET;
const uint32_t TOYOTA_PARAM_STOCK_LONGITUDINAL = 2U << TOYOTA_PARAM_OFFSET;
const uint32_t TOYOTA_PARAM_LTA = 4U << TOYOTA_PARAM_OFFSET;
const uint32_t TOYOTA_PARAM_GAS_INTERCEPTOR = 8U << TOYOTA_PARAM_OFFSET;

bool toyota_alt_brake = false;
bool toyota_stock_longitudinal = false;
bool toyota_lta = false;
int toyota_dbc_eps_torque_factor = 100;   // conversion factor for STEER_TORQUE_EPS in %: see dbc file

static uint32_t toyota_compute_checksum(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);
  uint8_t checksum = (uint8_t)(addr) + (uint8_t)((unsigned int)(addr) >> 8U) + (uint8_t)(len);
  for (int i = 0; i < (len - 1); i++) {
    checksum += (uint8_t)GET_BYTE(to_push, i);
  }
  return checksum;
}

static uint32_t toyota_get_checksum(CANPacket_t *to_push) {
  int checksum_byte = GET_LEN(to_push) - 1U;
  return (uint8_t)(GET_BYTE(to_push, checksum_byte));
}

static uint8_t toyota_get_counter(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);

  uint8_t cnt = 0U;
  if (addr == 0x201) {
    // Signal: COUNTER_PEDAL
    cnt = GET_BYTE(to_push, 4) & 0x0FU;
  }
  return cnt;
}

static bool toyota_get_quality_flag_valid(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);

  bool valid = false;
  if (addr == 0x260) {
    valid = GET_BIT(to_push, 3U) == 0U;  // STEER_ANGLE_INITIALIZING
  }
  return valid;
}

static void toyota_rx_hook(CANPacket_t *to_push) {
  if (GET_BUS(to_push) == 0U) {
    int addr = GET_ADDR(to_push);

    // get eps motor torque (0.66 factor in dbc)
    if (addr == 0x260) {
      int torque_meas_new = (GET_BYTE(to_push, 5) << 8) | GET_BYTE(to_push, 6);
      torque_meas_new = to_signed(torque_meas_new, 16);

      // scale by dbc_factor
      torque_meas_new = (torque_meas_new * toyota_dbc_eps_torque_factor) / 100;

      // update array of sample
      update_sample(&torque_meas, torque_meas_new);

      // increase torque_meas by 1 to be conservative on rounding
      torque_meas.min--;
      torque_meas.max++;

      // driver torque for angle limiting
      int torque_driver_new = (GET_BYTE(to_push, 1) << 8) | GET_BYTE(to_push, 2);
      torque_driver_new = to_signed(torque_driver_new, 16);
      update_sample(&torque_driver, torque_driver_new);

      // LTA request angle should match current angle while inactive, clipped to max accepted angle.
      // note that angle can be relative to init angle on some TSS2 platforms, LTA has the same offset
      bool steer_angle_initializing = GET_BIT(to_push, 3U) != 0U;
      if (!steer_angle_initializing) {
        int angle_meas_new = (GET_BYTE(to_push, 3) << 8U) | GET_BYTE(to_push, 4);
        angle_meas_new = CLAMP(to_signed(angle_meas_new, 16), -TOYOTA_LTA_MAX_ANGLE, TOYOTA_LTA_MAX_ANGLE);
        update_sample(&angle_meas, angle_meas_new);
      }
    }

    // enter controls on rising edge of ACC, exit controls on ACC off
    // exit controls on rising edge of gas press
    if (addr == 0x1D2) {
      // 5th bit is CRUISE_ACTIVE
      bool cruise_engaged = GET_BIT(to_push, 5U) != 0U;
      pcm_cruise_check(cruise_engaged);

      // sample gas pedal
      if (!enable_gas_interceptor) {
        gas_pressed = GET_BIT(to_push, 4U) == 0U;
      }
    }

    // sample speed
    if (addr == 0xaa) {
      int speed = 0;
      // sum 4 wheel speeds. conversion: raw * 0.01 - 67.67
      for (uint8_t i = 0U; i < 8U; i += 2U) {
        int wheel_speed = (GET_BYTE(to_push, i) << 8U) | GET_BYTE(to_push, (i + 1U));
        speed += wheel_speed - 6767;
      }
      // check that all wheel speeds are at zero value
      vehicle_moving = speed != 0;

      UPDATE_VEHICLE_SPEED(speed / 4.0 * 0.01 / 3.6);
    }

    // most cars have brake_pressed on 0x226, corolla and rav4 on 0x224
    if (((addr == 0x224) && toyota_alt_brake) || ((addr == 0x226) && !toyota_alt_brake)) {
      uint8_t bit = (addr == 0x224) ? 5U : 37U;
      brake_pressed = GET_BIT(to_push, bit) != 0U;
    }

    // sample gas interceptor
    if ((addr == 0x201) && enable_gas_interceptor) {
      int gas_interceptor = TOYOTA_GET_INTERCEPTOR(to_push);
      gas_pressed = gas_interceptor > TOYOTA_GAS_INTERCEPTOR_THRSLD;

      // TODO: remove this, only left in for gas_interceptor_prev test
      gas_interceptor_prev = gas_interceptor;
    }

    generic_rx_checks((addr == 0x2E4));
  }
}

static bool toyota_tx_hook(CANPacket_t *to_send) {
  bool tx = true;
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);

  // Check if msg is sent on BUS 0
  if (bus == 0) {

    // GAS PEDAL: safety check
    if (addr == 0x200) {
      if (longitudinal_interceptor_checks(to_send)) {
        tx = false;
      }
    }

    // ACCEL: safety check on byte 1-2
    if (addr == 0x343) {
      int desired_accel = (GET_BYTE(to_send, 0) << 8) | GET_BYTE(to_send, 1);
      desired_accel = to_signed(desired_accel, 16);

      bool violation = false;
      violation |= longitudinal_accel_checks(desired_accel, TOYOTA_LONG_LIMITS);

      // only ACC messages that cancel are allowed when openpilot is not controlling longitudinal
      if (toyota_stock_longitudinal) {
        bool cancel_req = GET_BIT(to_send, 24U) != 0U;
        if (!cancel_req) {
          violation = true;
        }
        if (desired_accel != TOYOTA_LONG_LIMITS.inactive_accel) {
          violation = true;
        }
      }

      if (violation) {
        tx = false;
      }
    }

    // AEB: block all actuation. only used when DSU is unplugged
    if (addr == 0x283) {
      // only allow the checksum, which is the last byte
      bool block = (GET_BYTES(to_send, 0, 4) != 0U) || (GET_BYTE(to_send, 4) != 0U) || (GET_BYTE(to_send, 5) != 0U);
      if (block) {
        tx = false;
      }
    }

    // LTA angle steering check
    if (addr == 0x191) {
      // check the STEER_REQUEST, STEER_REQUEST_2, TORQUE_WIND_DOWN, STEER_ANGLE_CMD signals
      bool lta_request = GET_BIT(to_send, 0U) != 0U;
      bool lta_request2 = GET_BIT(to_send, 25U) != 0U;
      int torque_wind_down = GET_BYTE(to_send, 5);
      int lta_angle = (GET_BYTE(to_send, 1) << 8) | GET_BYTE(to_send, 2);
      lta_angle = to_signed(lta_angle, 16);

      bool steer_control_enabled = lta_request || lta_request2;
      if (!toyota_lta) {
        // using torque (LKA), block LTA msgs with actuation requests
        if (steer_control_enabled || (lta_angle != 0) || (torque_wind_down != 0)) {
          tx = false;
        }
      } else {
        // check angle rate limits and inactive angle
        if (steer_angle_cmd_checks(lta_angle, steer_control_enabled, TOYOTA_STEERING_LIMITS)) {
          tx = false;
        }

        if (lta_request != lta_request2) {
          tx = false;
        }

        // TORQUE_WIND_DOWN is gated on steer request
        if (!steer_control_enabled && (torque_wind_down != 0)) {
          tx = false;
        }

        // TORQUE_WIND_DOWN can only be no or full torque
        if ((torque_wind_down != 0) && (torque_wind_down != 100)) {
          tx = false;
        }

        // check if we should wind down torque
        int driver_torque = MIN(ABS(torque_driver.min), ABS(torque_driver.max));
        if ((driver_torque > TOYOTA_LTA_MAX_DRIVER_TORQUE) && (torque_wind_down != 0)) {
          tx = false;
        }

        int eps_torque = MIN(ABS(torque_meas.min), ABS(torque_meas.max));
        if ((eps_torque > TOYOTA_LTA_MAX_MEAS_TORQUE) && (torque_wind_down != 0)) {
          tx = false;
        }
      }
    }

    // STEER: safety check on bytes 2-3
    if (addr == 0x2E4) {
      int desired_torque = (GET_BYTE(to_send, 1) << 8) | GET_BYTE(to_send, 2);
      desired_torque = to_signed(desired_torque, 16);
      bool steer_req = GET_BIT(to_send, 0U) != 0U;
      // When using LTA (angle control), assert no actuation on LKA message
      if (!toyota_lta) {
        if (steer_torque_cmd_checks(desired_torque, steer_req, TOYOTA_STEERING_LIMITS)) {
          tx = false;
        }
      } else {
        if ((desired_torque != 0) || steer_req) {
          tx = false;
        }
      }
    }
  }

  return tx;
}

static safety_config toyota_init(uint16_t param) {
  toyota_alt_brake = GET_FLAG(param, TOYOTA_PARAM_ALT_BRAKE);
  toyota_stock_longitudinal = GET_FLAG(param, TOYOTA_PARAM_STOCK_LONGITUDINAL);
  toyota_lta = GET_FLAG(param, TOYOTA_PARAM_LTA);
  enable_gas_interceptor = GET_FLAG(param, TOYOTA_PARAM_GAS_INTERCEPTOR);
  toyota_dbc_eps_torque_factor = param & TOYOTA_EPS_FACTOR;

  // Gas interceptor should not be used if openpilot is not controlling longitudinal
  if (toyota_stock_longitudinal) {
    enable_gas_interceptor = false;
  }

  safety_config ret;
  if (toyota_lta) {
    ret = enable_gas_interceptor ? BUILD_SAFETY_CFG(toyota_lta_interceptor_rx_checks, TOYOTA_INTERCEPTOR_TX_MSGS) : \
                                   BUILD_SAFETY_CFG(toyota_lta_rx_checks, TOYOTA_TX_MSGS);
  } else {
    ret = enable_gas_interceptor ? BUILD_SAFETY_CFG(toyota_lka_interceptor_rx_checks, TOYOTA_INTERCEPTOR_TX_MSGS) : \
                                   BUILD_SAFETY_CFG(toyota_lka_rx_checks, TOYOTA_TX_MSGS);
  }
  return ret;
}

static int toyota_fwd_hook(int bus_num, int addr) {

  int bus_fwd = -1;

  if (bus_num == 0) {
    bus_fwd = 2;
  }

  if (bus_num == 2) {
    // block stock lkas messages and stock acc messages (if OP is doing ACC)
    // in TSS2, 0x191 is LTA which we need to block to avoid controls collision
    int is_lkas_msg = ((addr == 0x2E4) || (addr == 0x412) || (addr == 0x191));
    // in TSS2 the camera does ACC as well, so filter 0x343
    int is_acc_msg = (addr == 0x343);
    int block_msg = is_lkas_msg || (is_acc_msg && !toyota_stock_longitudinal);
    if (!block_msg) {
      bus_fwd = 0;
    }
  }

  return bus_fwd;
}

const safety_hooks toyota_hooks = {
  .init = toyota_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .fwd = toyota_fwd_hook,
  .get_checksum = toyota_get_checksum,
  .compute_checksum = toyota_compute_checksum,
  .get_counter = toyota_get_counter,
  .get_quality_flag_valid = toyota_get_quality_flag_valid,
};
