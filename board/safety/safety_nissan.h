

const int NISSAN_MAX_STEER = 2047; // 1s
// real time torque limit to prevent controls spamming
// the real time limit is 1500/sec
const int NISSAN_MAX_RT_DELTA = 940;          // max delta torque allowed for real time checks
const uint32_t NISSAN_RT_INTERVAL = 250000;    // 250ms between real time checks
const int NISSAN_MAX_RATE_UP = 50;
const int NISSAN_MAX_RATE_DOWN = 70;
const int NISSAN_DRIVER_TORQUE_ALLOWANCE = 60;
const int NISSAN_DRIVER_TORQUE_FACTOR = 10;

float nissan_speed = 0;
int nissan_controls_allowed_last = 0;
int nissan_angle_meas_new = 0;
uint32_t nissan_ts_angle_last = 0;
int nissan_cruise_engaged_last = 0;
int nissan_rt_angle_last = 0;
int nissan_desired_angle_last = 0;
uint32_t nissan_ts_last = 0;
struct sample_t nissan_torque_driver; // last few driver torques measured


const struct lookup_t NISSAN_LOOKUP_ANGLE_RATE_UP = {
  {2., 7., 17.},
  {5., .8, .15}};

const struct lookup_t NISSAN_LOOKUP_ANGLE_RATE_DOWN = {
  {2., 7., 17.},
  {5., 3.5, .4}};

// TODO: Need to work out the max angle at certain speeds
const struct lookup_t NISSAN_LOOKUP_MAX_ANGLE = {
    {2., 29., 38.},
    {500., 500., 500.}};


static void nissan_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if (bus == 0) {
    if (addr == 0x2) {
      // Current steering angle
      // Factor -0.1, little endian
      nissan_angle_meas_new = to_signed(to_push->RDLR & 0xFFFF, 16) * -0.1;
    }
    
    if (addr == 0x29a) {
      // Get current speed
      // Factor 0.00555
      nissan_speed = (((to_push->RDLR >> 8) & 0xFF00) | ((to_push->RDLR >> 24) & 0xFF)) * 0.00555 / 3.6;
    }
  }
  
  if (bus == 1) {
    if (addr == 0x1b6) {
      int cruise_engaged = (to_push->RDHR >> 6) & 1;
      if (cruise_engaged && !nissan_cruise_engaged_last) {
        controls_allowed = 1;
      } else if (!cruise_engaged) {
        controls_allowed = 0;
      }
      nissan_cruise_engaged_last = cruise_engaged;

    }
  }
}

static int nissan_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int tx = 1;
  int addr = GET_ADDR(to_send);

  // steer cmd checks
  if (addr == 0x169) { // Factor -0.01, offeset 1310
    float desired_angle = (((to_send->RDLR << 10) & 0x3FC00) | ((to_send->RDLR >> 6) & 0x3FC) | ((to_send->RDLR >> 16) & 0x3)) * -0.01 + 1310;
    desired_angle = to_signed(desired_angle, 18);
    bool violation = 0;
    uint32_t ts = TIM2->CNT;
    bool lka_active = ((to_send->RDHR >> 20) & 1);

    if (controls_allowed) {
      uint32_t ts_elapsed = get_ts_elapsed(ts, nissan_ts_angle_last);

      // *** angle real time check
      // add 1 to not false trigger the violation and multiply by 25 since the check is done every 250ms and steer angle is updated at 100Hz
      int rt_delta_angle_up = ((int)(((interpolate(NISSAN_LOOKUP_ANGLE_RATE_UP, speed) * 25. * CAN_TO_DEG) + 1.)));
      int rt_delta_angle_down = ((int)(((interpolate(NISSAN_LOOKUP_ANGLE_RATE_DOWN, speed) * 25. * CAN_TO_DEG) + 1.)));
      int highest_rt_angle = nissan_rt_angle_last + ((nissan_rt_angle_last > 0) ? rt_delta_angle_up : rt_delta_angle_down);
      int lowest_rt_angle = nissan_rt_angle_last - ((nissan_rt_angle_last > 0) ? rt_delta_angle_down : rt_delta_angle_up);

      // Limit maximum steering angle at current speed
      float maximum_angle = interpolate(NISSAN_LOOKUP_MAX_ANGLE, nissan_speed);

      if (highest_rt_angle > maximum_angle) {
        highest_rt_angle = maximum_angle;
      } else if (lowest_rt_angle < -maximum_angle) {
        lowest_rt_angle = -maximum_angle;
      }

      if ((ts_elapsed > NISSAN_RT_INTERVAL) || (controls_allowed && !nissan_controls_allowed_last))
      {
        nissan_rt_angle_last = nissan_angle_meas_new;
        nissan_ts_angle_last = ts;
      }

      // check for violation;
      violation |= max_limit_check(nissan_angle_meas_new, highest_rt_angle, lowest_rt_angle);

      nissan_controls_allowed_last = controls_allowed;
    }

    // no lka_enabled bit if controls not allowed
    if (!controls_allowed && lka_active) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      nissan_desired_angle_last = 0;
      nissan_rt_angle_last = 0;
      nissan_ts_last = ts;
    }

    if (violation) {
      tx = 0;
    }
  }
  return tx;
}


static int nissan_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {

  int bus_fwd = -1;
  if (bus_num == 0) {
      bus_fwd = 2;  // ADAS
  }
  if (bus_num == 2) {
    // 0x169 is LKAS
    int addr = GET_ADDR(to_fwd);
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
  .ignition = default_ign_hook,
  .fwd = nissan_fwd_hook,
};
