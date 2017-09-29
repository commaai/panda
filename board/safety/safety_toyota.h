// start with random large value (300kph) so torque is conservatively limited
// until a valid speed measurement is received
int32_t speed = 30000;
// 2 speed thresholds with 2 different steer torque levels allowed
const int32_t SPEED_0 = 2100;       // 16 kph/10 mph + 5 kph margin VS carcontroller
const int32_t SPEED_1 = 5000;       // 45 kph/28 mph + 5 kph margin VS carcontroller
const int32_t MAX_STEER_0 = 1500;   // max
const int32_t MAX_STEER_1 = 1000;   // reduced
const int16_t MAX_ACCEL = 1500;     // 1.5 m/s2
const int16_t MIN_ACCEL = -3000;    // 3.0 m/s2
int actuation_limits = 1;              // by default steer limits are imposed

static void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  // get the up to date speed
  if ((to_push->RIR>>21) == 0xb4) {
    // unit is 0.01 kph (see dbc file)
    speed = ((to_push->RDHR) & 0xFF00) | ((to_push->RDHR >> 16) & 0xFF);
  }

  // exit controls on ACC off
  if ((to_push->RIR>>21) == 0x1D2) {
    // 4 bits: 55-52
    if (to_push->RDHR & 0xF00000) {
      controls_allowed = 1;
    }
    else {
      controls_allowed = 0;
    }
  }
}

static int toyota_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

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

    // consider absolute value
    if (desired_torque < 0) {
      desired_torque *= -1;
    }

    // only check if controls are allowed and actuation_limits are imposed
    if (controls_allowed && actuation_limits) {
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
      if (desired_torque > max_steer) {
        return 0;
      }

    } else if (!controls_allowed && (desired_torque != 0)) {
      return 0;
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
