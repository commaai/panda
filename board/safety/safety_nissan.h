
const uint32_t NISSAN_RT_INTERVAL = 250000;    // 250ms between real time checks

const struct lookup_t NISSAN_LOOKUP_ANGLE_RATE_UP = {
  {2., 7., 17.},
  {5., .8, .15}};

const struct lookup_t NISSAN_LOOKUP_ANGLE_RATE_DOWN = {
  {2., 7., 17.},
  {5., 3.5, .4}};

const struct lookup_t NISSAN_LOOKUP_MAX_ANGLE = {
  {3.3, 12, 32},
  {540., 120., 23.}};

const AddrBus NISSAN_TX_MSGS[] = {{0x169, 0}, {0x20b, 2}};

AddrCheckStruct nissan_rx_checks[] = {
  {.addr = {0x2}, .bus = 0, .expected_timestep = 100000U},
  {.addr = {0x29a}, .bus = 0, .expected_timestep = 50000U},
  {.addr = {0x20b}, .bus = 0, .expected_timestep = 50000U},
  {.addr = {0x1b6}, .bus = 1, .expected_timestep = 100000U},
};
const int NISSAN_RX_CHECK_LEN = sizeof(nissan_rx_checks) / sizeof(nissan_rx_checks[0]);

float nissan_speed = 0;
int nissan_controls_allowed_last = 0;
uint32_t nissan_ts_angle_last = 0;
int nissan_cruise_engaged_last = 0;
int nissan_desired_angle_last = 0;

struct sample_t angle_meas;            // last 3 steer angles


static int nissan_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  bool valid = addr_safety_check(to_push, nissan_rx_checks, NISSAN_RX_CHECK_LEN,
                                 NULL, NULL, NULL);

  if (valid) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    if (bus == 0) {
      if (addr == 0x2) {
        // Current steering angle
        // Factor -0.1, little endian
        int angle_meas_new = (GET_BYTES_04(to_push) & 0xFFFF);
        angle_meas_new = to_signed(angle_meas_new, 16) * -0.1;

        // update array of samples
        update_sample(&angle_meas, angle_meas_new);
      }

      if (addr == 0x29a) {
        // Get current speed
        // Factor 0.00555
        nissan_speed = ((GET_BYTE(to_push, 2) << 8) | (GET_BYTE(to_push, 3))) * 0.00555 / 3.6;
      }
    }

    if (bus == 1) {
      if (addr == 0x1b6) {
        int cruise_engaged = (GET_BYTE(to_push, 4) >> 6) & 1;
        if (cruise_engaged && !nissan_cruise_engaged_last) {
          controls_allowed = 1;
        }
        if (!cruise_engaged) {
          controls_allowed = 0;
        }
        nissan_cruise_engaged_last = cruise_engaged;
      }
    }
  }
  return valid;
}


static int nissan_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int tx = 1;
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);

  if (!msg_allowed(addr, bus, NISSAN_TX_MSGS, sizeof(NISSAN_TX_MSGS) / sizeof(NISSAN_TX_MSGS[0]))) {
    tx = 0;
  }

  if (relay_malfunction) {
    tx = 0;
  }

  // steer cmd checks
  if (addr == 0x169) {
    float desired_angle = ((GET_BYTE(to_send, 0) << 10) | (GET_BYTE(to_send, 1) << 2) | (GET_BYTE(to_send, 2) & 0x3));
    desired_angle = to_signed(desired_angle, 18);

    // //scale by dbc factor -0.01, offeset 1310
    // desired_angle =  (desired_angle * -0.01) + 1310;

    bool violation = 0;
    uint32_t ts = TIM2->CNT;
    bool lka_active = (GET_BYTE(to_send, 6) >> 4) & 1;

    if (controls_allowed) {
      uint32_t ts_elapsed = get_ts_elapsed(ts, nissan_ts_angle_last);

      // *** angle real time check
      // add 1 to not false trigger the violation and multiply by 25 since the check is done every 250ms and steer angle is updated at 100Hz
      int rt_delta_angle_up = ((((interpolate(NISSAN_LOOKUP_ANGLE_RATE_UP, speed) * 25.) + 1.)));
      int rt_delta_angle_down = ((((interpolate(NISSAN_LOOKUP_ANGLE_RATE_DOWN, speed) * 25.) + 1.)));
      int highest_desired_angle = nissan_desired_angle_last + ((nissan_desired_angle_last > 0) ? rt_delta_angle_up : rt_delta_angle_down);
      int lowest_desired_angle = nissan_desired_angle_last - ((nissan_desired_angle_last > 0) ? rt_delta_angle_down : rt_delta_angle_up);

      // Limit maximum steering angle at current speed 
      int maximum_angle = ((int)interpolate(NISSAN_LOOKUP_MAX_ANGLE, nissan_speed));

      if (highest_desired_angle > maximum_angle) {
        highest_desired_angle = maximum_angle;
      }
      if (lowest_desired_angle < -maximum_angle) {
        lowest_desired_angle = -maximum_angle;
      }

      if ((ts_elapsed > NISSAN_RT_INTERVAL) || (controls_allowed && !nissan_controls_allowed_last))
      {
        nissan_desired_angle_last = desired_angle;
        nissan_ts_angle_last = ts;
      }

      // check for violation;
      violation |= max_limit_check(desired_angle, highest_desired_angle, lowest_desired_angle);

      nissan_controls_allowed_last = controls_allowed;
    }

    // desired steer angle should be the same as steer angle measured when controls are off
    if ((!controls_allowed) &&
          ((desired_angle < ((float)angle_meas.min - 1)) ||
          (desired_angle > ((float)angle_meas.max + 1)))) {
      violation = 1;
    }

    // no lka_enabled bit if controls not allowed
    if (!controls_allowed && lka_active) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      nissan_desired_angle_last = 0;
    }

    if (violation) {
      tx = 0;
    }
  }
  return tx;
}


static int nissan_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  int bus_fwd = -1;
  int addr = GET_ADDR(to_fwd);

  if (bus_num == 0) {
    // 0x20b is CruiseThrottle
    int block_msg = (addr == 0x20b);
    if (!block_msg) {
      bus_fwd = 2;  // ADAS
    }
  }
  if (bus_num == 2) {
    // 0x169 is LKAS
    int block_msg = (addr == 0x169);
    if (!block_msg) {
      bus_fwd = 0;  // V-CAN
    }
  }

  // fallback to do not forward
  return bus_fwd;
}

const safety_hooks nissan_hooks = {
  .init = nooutput_init,
  .rx = nissan_rx_hook,
  .tx = nissan_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = nissan_fwd_hook,
  .addr_check = nissan_rx_checks,
  .addr_check_len = sizeof(nissan_rx_checks) / sizeof(nissan_rx_checks[0]),
};
