// IPAS override
const int32_t IPAS_OVERRIDE_THRESHOLD = 200;  // disallow controls when user torque exceeds this value

// global torque limit
const int32_t MAX_TORQUE = 1500;       // max torque cmd allowed ever

// rate based torque limit + stay within actually applied
// packet is sent at 100hz, so this limit is 1000/sec
const int32_t MAX_RATE_UP = 10;        // ramp up slow
const int32_t MAX_RATE_DOWN = 25;      // ramp down fast
const int32_t MAX_TORQUE_ERROR = 350;  // max torque cmd in excess of torque motor

//double MAX_ANGLE_RATE_V[3] = {0., 5., 15};
//double MAX_ANGLE_RATE_UP[] = {5., .8, .15};
//double MAX_ANGLE_RATE_DOWN[] = {5., 3.5, .4};

struct Lookup {
  double x[3];
  double y[3];
};

const struct Lookup LOOKUP_ANGLE_RATE_UP = {
  {0., 5., 15.},
  {5., .8, .15}};

const struct Lookup LOOKUP_ANGLE_RATE_DOWN = {
  {0., 5., 15.},
  {5., 3.5, .4}};

// real time torque limit to prevent controls spamming
// the real time limit is 1500/sec
const int32_t MAX_RT_DELTA = 375;      // max delta torque allowed for real time checks
const int32_t RT_INTERVAL = 250000;    // 250ms between real time checks

// longitudinal limits
const int16_t MAX_ACCEL = 1500;        // 1.5 m/s2
const int16_t MIN_ACCEL = -3000;       // 3.0 m/s2

int cruise_engaged_last = 0;           // cruise state
int ipas_state = 1;                    // 1 disabled, 3 executing angle control, 5 override
int angle_control = 0;                 // 1 if direct angle control packets are seen
double speed = 0.;

// track the torque measured for limiting
int16_t torque_meas[3] = {0, 0, 0};    // last 3 motor torques produced by the eps
int16_t torque_meas_min = 0, torque_meas_max = 0;
int16_t angle_meas[3] = {0, 0, 0};     // last 3 steer angles
int16_t angle_meas_min = 0, angle_meas_max = 0;
int16_t torque_driver[3] = {0, 0, 0};    // last 3 driver steering torque
int16_t torque_driver_min = 0, torque_driver_max = 0;

// global actuation limit state
int actuation_limits = 1;              // by default steer limits are imposed
int16_t dbc_eps_torque_factor = 100;   // conversion factor for STEER_TORQUE_EPS in %: see dbc file

// state of torque limits
int16_t desired_torque_last = 0;       // last desired steer torque
int16_t rt_torque_last = 0;            // last desired torque for real time check
int16_t rt_angle_last = 0;             // last desired torque for real time check
uint32_t ts_last = 0;
uint32_t ts_angle_last = 0;

int to_signed(int d, int bits) {
  if (d >= (1 << (bits - 1))) {
    d -= (1 << bits);
  }
  return d;
}

// interp function that holds extreme values
double interpolate(struct Lookup xy, double x) {
  int size = sizeof(xy.x) / sizeof(xy.x[0]);
  if (x <= xy.x[0]) {
    return xy.y[0];
  }

  else if (x > xy.x[size - 1]){
    return xy.y[size - 1];

  } else {
    for (int i=0; i < size-1; i++) {
      if (x > xy.x[i]) {
        return (xy.y[i+1] - xy.y[i]) / (xy.x[i+1] - xy.x[i]);
      }
    }   
  }
  return 0;
}

double get_speed(CAN_FIFOMailBox_TypeDef *to_push) {
    int32_t wheel1 = (((to_push->RDLR & 0xFF00) >> 8) | ((to_push->RDLR & 0xFF)) << 8);
    int32_t wheel2 = (((to_push->RDLR & 0xFF000000) >> 24) | ((to_push->RDLR & 0xFF0000)) >> 8);
    int32_t wheel3 = (((to_push->RDHR & 0xFF00) >> 8) | ((to_push->RDHR & 0xFF)) << 8);
    int32_t wheel4 = (((to_push->RDHR & 0xFF000000) >> 24) | ((to_push->RDHR & 0xFF0000)) >> 8);
    // units are 0.01 kph
    return ((double)(wheel1 + wheel2 + wheel3 + wheel4)) * (100. / 4. / 3.6);
}

uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  return ts > ts_last ? ts - ts_last : (0xFFFFFFFF - ts_last) + 1 + ts;
}

static void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  // EPS torque sensor
  if ((to_push->RIR>>21) == 0x260) {
    // get eps motor torque (see dbc_eps_torque_factor in dbc)
    int16_t torque_meas_new_16 = (((to_push->RDHR) & 0xFF00) | ((to_push->RDHR >> 16) & 0xFF));

    // increase torque_meas by 1 to be conservative on rounding
    int torque_meas_new = ((int)(torque_meas_new_16) * dbc_eps_torque_factor / 100) + (torque_meas_new_16 > 0 ? 1 : -1);

    // shift the array
    for (int i = sizeof(torque_meas)/sizeof(torque_meas[0]) - 1; i > 0; i--) {
      torque_meas[i] = torque_meas[i-1];
    }
    torque_meas[0] = torque_meas_new;

    // get the minimum and maximum measured torque over the last 3 frames
    torque_meas_min = torque_meas_max = torque_meas[0];
    for (int i = 1; i < sizeof(torque_meas)/sizeof(torque_meas[0]); i++) {
      if (torque_meas[i] < torque_meas_min) torque_meas_min = torque_meas[i];
      if (torque_meas[i] > torque_meas_max) torque_meas_max = torque_meas[i];
    }

    // get driver steering torque
    int16_t torque_driver_new = (((to_push->RDLR) & 0xFF00) | ((to_push->RDLR >> 16) & 0xFF));

    // shift the array
    for (int i = sizeof(torque_driver)/sizeof(torque_driver[0]) - 1; i > 0; i--) {
      torque_driver[i] = torque_driver[i-1];
    }
    torque_driver[0] = torque_driver_new;

    // get the minimum and maximum driver torque over the last 3 frames
    torque_driver_min = torque_driver_max = torque_driver[0];
    for (int i = 1; i < sizeof(torque_driver)/sizeof(torque_driver[0]); i++) {
      if (torque_driver[i] < torque_driver_min) torque_driver_min = torque_driver[i];
      if (torque_driver[i] > torque_driver_max) torque_driver_max = torque_driver[i];
    }
  }

  // get steer angle
  if ((to_push->RIR>>21) == 0x25) {
    int angle_meas_new = ((to_push->RDLR & 0xf) << 8) + ((to_push->RDLR & 0xff00) >> 8);
    uint32_t ts = TIM2->CNT;

    angle_meas_new = to_signed(angle_meas_new, 12);

    // shift the array
    for (int i = sizeof(angle_meas)/sizeof(angle_meas[0]) - 1; i > 0; i--) {
      angle_meas[i] = angle_meas[i-1];
    }
    angle_meas[0] = angle_meas_new;

    // get the minimum and maximum measured angle over the last 3 frames
    angle_meas_min = angle_meas_max = angle_meas[0];
    for (int i = 1; i < sizeof(angle_meas)/sizeof(angle_meas[0]); i++) {
      if (angle_meas[i] < angle_meas_min) angle_meas_min = angle_meas[i];
      if (angle_meas[i] > angle_meas_max) angle_meas_max = angle_meas[i];
    }

    // *** angle ral time check
    double rt_delta_angle_up = interpolate(LOOKUP_ANGLE_RATE_UP, speed) * 25. * 3./2. + 1.;
    double rt_delta_angle_down = interpolate(LOOKUP_ANGLE_RATE_DOWN, speed) * 25 * 3. / 2. + 1.;
    int16_t highest_rt_angle = rt_angle_last + (rt_angle_last > 0? rt_delta_angle_up:rt_delta_angle_down);
    int16_t lowest_rt_angle = rt_angle_last - (rt_angle_last > 0? rt_delta_angle_down:rt_delta_angle_up);

    // check for violation
    if ((angle_meas_new < lowest_rt_angle) ||
        (angle_meas_new > highest_rt_angle)) {
      controls_allowed = 0;
    }

    // every RT_INTERVAL set the new limits
    uint32_t ts_elapsed = get_ts_elapsed(ts, ts_angle_last);
    if (ts_elapsed > RT_INTERVAL) {
      rt_angle_last = angle_meas_new;
      ts_angle_last = ts;
    }

  }

  // get speed
  if ((to_push->RIR>>21) == 0xaa) {
    speed = get_speed(to_push);
    //printf("speed %f\n", speed);
  }

  // enter controls on rising edge of ACC, exit controls on ACC off
  if ((to_push->RIR>>21) == 0x1D2) {
    // 4 bits: 55-52
    int cruise_engaged = to_push->RDHR & 0xF00000;
    if (cruise_engaged && (!cruise_engaged_last)) {
      controls_allowed = 1;
    } else if (!cruise_engaged) {
      controls_allowed = 0;
    }
    cruise_engaged_last = cruise_engaged;
  }

  // get ipas state
  if ((to_push->RIR>>21) == 0x262) {
    ipas_state = (to_push->RDLR & 0xf);
  }

  // exit controls on high steering override
  if (angle_control && ((torque_driver_min > IPAS_OVERRIDE_THRESHOLD) ||
                        (torque_driver_max < -IPAS_OVERRIDE_THRESHOLD) ||
                        (ipas_state==5))) {
    controls_allowed = 0;
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

    // STEER ANGLE
    if ((to_send->RIR>>21) == 0x266) {

      angle_control = 1;   // we are in angle control mode
      int desired_angle = ((to_send->RDLR & 0xf) << 8) + ((to_send->RDLR & 0xff00) >> 8);
      int ipas_state_cmd = ((to_send->RDLR & 0xff) >> 4);
      int16_t violation = 0;

      desired_angle = to_signed(desired_angle, 12);

      // desired steer angle should be the same as steer angle measured when controls are off
      if ((!controls_allowed) && 
           ((desired_angle < (angle_meas_min - 1)) ||
            (desired_angle > (angle_meas_max + 1)) ||
            (ipas_state_cmd != 1))) {
        violation = 1;
      }

      if (violation) {
        return false;
      }
    }

    // STEER TORQUE: safety check on bytes 2-3
    if ((to_send->RIR>>21) == 0x2E4) {
      int16_t desired_torque = (to_send->RDLR & 0xFF00) | ((to_send->RDLR >> 16) & 0xFF);
      int16_t violation = 0;

      uint32_t ts = TIM2->CNT;

      // only check if controls are allowed and actuation_limits are imposed
      if (controls_allowed && actuation_limits) {

        // *** global torque limit check ***
        if (desired_torque < -MAX_TORQUE) violation = 1;
        if (desired_torque > MAX_TORQUE) violation = 1;


        // *** torque rate limit check ***
        int16_t highest_allowed_torque = max(desired_torque_last, 0) + MAX_RATE_UP;
        int16_t lowest_allowed_torque = min(desired_torque_last, 0) - MAX_RATE_UP;

        // if we've exceeded the applied torque, we must start moving toward 0
        highest_allowed_torque = min(highest_allowed_torque, max(desired_torque_last - MAX_RATE_DOWN, max(torque_meas_max, 0) + MAX_TORQUE_ERROR));
        lowest_allowed_torque = max(lowest_allowed_torque, min(desired_torque_last + MAX_RATE_DOWN, min(torque_meas_min, 0) - MAX_TORQUE_ERROR));

        // check for violation
        if ((desired_torque < lowest_allowed_torque) || (desired_torque > highest_allowed_torque)) {
          violation = 1;
        }

        // used next time
        desired_torque_last = desired_torque;


        // *** torque real time rate limit check ***
        int16_t highest_rt_torque = max(rt_torque_last, 0) + MAX_RT_DELTA;
        int16_t lowest_rt_torque = min(rt_torque_last, 0) - MAX_RT_DELTA;

        // check for violation
        if ((desired_torque < lowest_rt_torque) || (desired_torque > highest_rt_torque)) {
          violation = 1;
        }

        // every RT_INTERVAL set the new limits
        uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
        if (ts_elapsed > RT_INTERVAL) {
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
        return false;
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

static void toyota_init(int16_t param) {
  controls_allowed = 0;
  actuation_limits = 1;
  dbc_eps_torque_factor = param;
}

static int toyota_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  return -1;
}

const safety_hooks toyota_hooks = {
  .init = toyota_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
  .fwd = toyota_fwd_hook,
};

static void toyota_nolimits_init(int16_t param) {
  controls_allowed = 0;
  actuation_limits = 0;
  dbc_eps_torque_factor = param;
}

const safety_hooks toyota_nolimits_hooks = {
  .init = toyota_nolimits_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
  .fwd = toyota_fwd_hook,
};
