// start with random large value (300kph) so torque is conservatively limited
// until a valid speed measurement is received
int32_t speed = 30000;
int16_t torque_meas[3] = {0, 0, 0};    // last 3 motor torques produced by the eps
const int32_t SPEED_0 = 2100;          // 16 kph/10 mph + 5 kph margin VS carcontroller
const int32_t SPEED_1 = 5000;          // 45 kph/28 mph + 5 kph margin VS carcontroller
const int32_t MAX_STEER_0 = 1500;      // max
const int32_t MAX_STEER_1 = 1500;      // max, keeping flexibility for future applications
const int32_t MAX_RT_DELTA = 500;      // max delta torque allowed for real time checks
const int32_t RT_INTERVAL = 250000;    // 250ms between real time checks
const int32_t MAX_STEER_ERROR = 300;   // max torque cmd in excess of torque motor
const int32_t MAX_RATE_UP = 8;         // ramp up slow
const int32_t MAX_RATE_DOWN = 45;      // ramp down fast
const int16_t MAX_ACCEL = 1500;        // 1.5 m/s2
const int16_t MIN_ACCEL = -3000;       // 3.0 m/s2
int actuation_limits = 1;              // by default steer limits are imposed
int16_t desired_torque_last = 0;       // last desired steer torque
int16_t rt_torque_last = 0;            // last desired torque for real time check
uint32_t ts_last = 0;

static void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  // get vehicle speed
  if ((to_push->RIR>>21) == 0xb4) {
    // unit is 0.01 kph (see dbc file)
    speed = ((to_push->RDHR) & 0xFF00) | ((to_push->RDHR >> 16) & 0xFF);
  }

  // get eps motor torque (0.66 factor in dbc)
  if ((to_push->RIR>>21) == 0x260) {
    int16_t torque_meas_new = (((to_push->RDHR) & 0xFF00) | ((to_push->RDHR >> 16) & 0xFF));
    // increase torque_meas by 1 to be conservative on rounding
    if (torque_meas_new > 0) {
      torque_meas_new = (torque_meas_new / 3 + 1) * 2;
    } else {
      torque_meas_new = (torque_meas_new / 3 - 1) * 2;
    }
    int8_t len = sizeof(torque_meas)/sizeof(torque_meas[0]);
    for (int i = len - 1; i > 0; i--) {
      torque_meas[i] = torque_meas[i-1];
    }
    torque_meas[0] = torque_meas_new;
  }


  // exit controls on ACC off
  if ((to_push->RIR>>21) == 0x1D2) {
    // 4 bits: 55-52
    if (to_push->RDHR & 0xF00000) {
      controls_allowed = 1;
    } else {
      controls_allowed = 0;
    }
  }
}

static int toyota_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  // Check if msg is sent on BUS 0
  if (((to_send->RDTR >> 4) & 0xF) == 0) {

    // ACCEL: safety check on byte 1-2
    if ((to_send->RIR>>21) == 0x343) {
      int16_t desired_accel = ((to_send->RDLR & 0xFF) << 8) | ((to_send->RDLR >> 8) & 0xFF);
      if (controls_allowed && actuation_limits) {
        if ((desired_accel > MAX_ACCEL) || (desired_accel < MIN_ACCEL)) {
          return 0;
        }
      } else if (!controls_allowed && (desired_accel != 0)) {
        return 0;
      }
    }

    // STEER: safety check on bytes 2-3
    if ((to_send->RIR>>21) == 0x2E4) {
      int16_t desired_torque = (to_send->RDLR & 0xFF00) | ((to_send->RDLR >> 16) & 0xFF);
      int16_t violation = 0;
      uint32_t ts = TIM2->CNT;

      // only check if controls are allowed and actuation_limits are imposed
      if (controls_allowed && actuation_limits) {

        // *** max torque check ***

        int32_t max_steer = 0;
        // speed dependent limitation
        if (speed < SPEED_0) {
          max_steer = MAX_STEER_0;
        } else if (speed > SPEED_1) {
          max_steer = MAX_STEER_1;
        } else {
          // linear interp
          max_steer = MAX_STEER_0 - ((speed - SPEED_0) * (MAX_STEER_0 - MAX_STEER_1)) / (SPEED_1 - SPEED_0);
        }
        if ((desired_torque > max_steer) || (desired_torque < -max_steer)) {
          violation = 1;
        }

        // *** steer rate limit check ***

        // get min and max torque_meas among the latest samples
        int16_t torque_meas_min = 0;
        int16_t torque_meas_max = 0;
        int16_t rate_down = 0;
        int16_t rate_up = 0;
        int16_t lim_min = 0;
        int16_t lim_max = 0;
         
        int8_t len = sizeof(torque_meas)/sizeof(torque_meas[0]);

        for (int i = 0; i < len; i++) {
          if (torque_meas[i] < torque_meas_min) {
            torque_meas_min = torque_meas[i];
          }
          if (torque_meas[i] > torque_meas_max) {
            torque_meas_max = torque_meas[i];
          }
        }

        // add allowance
        torque_meas_min -= MAX_STEER_ERROR;
        torque_meas_max += MAX_STEER_ERROR;

        // desired_torque has to be:
        // - within the rate limits
        // - subject to the rate limit, not above the measured torque plus allowances
        if (desired_torque_last > 0) {
          // give the rate limits only, these are the boundaries
          rate_up = desired_torque_last + MAX_RATE_UP;
          rate_down = desired_torque_last - MAX_RATE_DOWN;

          // create final limits, considering the measured motor torque
          lim_min = rate_down;
          lim_max =  torque_meas_max > lim_min ? torque_meas_max : lim_min;
          lim_max = lim_max < rate_up ? lim_max : rate_up;
        } else {
          // give the rate limits only, these are the boundaries
          rate_up = desired_torque_last + MAX_RATE_DOWN;
          rate_down = desired_torque_last - MAX_RATE_UP;

          // create final limits, considering the measured motor torque
          lim_max = rate_up;
          lim_min =  torque_meas_min < lim_max ? torque_meas_min : lim_max;
          lim_min = lim_min > rate_down ? lim_min : rate_down;
        }
        
        if ((desired_torque < lim_min) || (desired_torque > lim_max)) {
          violation = 1;
        }

        // *** steer real time rate limit check ***

        int16_t max_rt_torque = 0;
        int16_t min_rt_torque = 0;
        // check every RT_INTERVAL if steer torque increased by more than MAX_RT_DELTA units in the positive directions
        if (ts > ts_last) {
          uint32_t ts_elapsed = ts > ts_last ? ts - ts_last : (0xFFFFFFFF - ts_last) + ts;
          if (ts_elapsed > RT_INTERVAL) {
            ts_last = ts;
            if (rt_torque_last > 0) {
              max_rt_torque = rt_torque_last + MAX_RT_DELTA;
              min_rt_torque = - MAX_RT_DELTA;
            } else {
              max_rt_torque = MAX_RT_DELTA;
              min_rt_torque = rt_torque_last - MAX_RT_DELTA;
            }
            if ((desired_torque < min_rt_torque) || (desired_torque > max_rt_torque)) {
              violation = 1;
            }
          }
        }


      } else if (!controls_allowed && (desired_torque != 0)) {
        violation = 1;
      }

      if (violation) {
        desired_torque_last = 0;  // start back from 0
        rt_torque_last = 0;       // start back from 0
        return 0;
      } else {
        desired_torque_last = desired_torque;
        rt_torque_last = desired_torque;
      }
    }
  }

  // 1 allows the message through
  return true;
}

static int toyota_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  // TODO: add safety if using LIN
  return true;
}

static void toyota_init() {
  controls_allowed = 0;
  actuation_limits = 1;
}

const safety_hooks toyota_hooks = {
  .init = toyota_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
};

static void toyota_nolimits_init() {
  controls_allowed = 0;
  actuation_limits = 0;
}

const safety_hooks toyota_nolimits_hooks = {
  .init = toyota_nolimits_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
};
