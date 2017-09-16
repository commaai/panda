// start with random large value (300kph) so torque is conservatively limited
// until a valid speed measurement is received
int32_t speed = 30000;
// 2 speed thresholds with 2 different steer torque levels allowed
const int32_t SPEED_0 = 2100;       // 16 kph/10 mph + 5 kph margin VS carcontroller
const int32_t SPEED_1 = 5000;       // 45 kph/28 mph + 5 kph margin VS carcontroller
const int32_t MAX_STEER_0 = 1500;   // max
const int32_t MAX_STEER_1 = 1000;   // reduced
int torque_limits = 1;              // by default steer limits are imposed

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

  // STEER: safety check on bytes 2-3
  if ((to_send->RIR>>21) == 0x2E4) {
    int16_t desired_torque = (to_send->RDLR & 0xFF00) | ((to_send->RDLR >> 16) & 0xFF);

    // consider absolute value
    if (desired_torque < 0) {
      desired_torque *= -1;
    }

    // only check if controls are allowed and torque_limits are imposed
    if (controls_allowed && torque_limits) {
      // speed dependent limitation
      if ((speed < SPEED_0) && (desired_torque > MAX_STEER_0)) {
        return 0;
      } else if ((speed > SPEED_1) && (desired_torque > MAX_STEER_1)) {
        return 0;
      } else {
        // linear interp
        int32_t max_steer = MAX_STEER_0 - ((speed - SPEED_0) * (MAX_STEER_0 - MAX_STEER_1)) / (SPEED_1 - SPEED_0);
        if (desired_torque > max_steer) {
          return 0;
        }
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
  torque_limits = 1;
}

const safety_hooks toyota_hooks = {
  .init = toyota_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
};

static void toyota_nolimits_init() {
  controls_allowed = 0;
  torque_limits = 0;
}

const safety_hooks toyota_nolimits_hooks = {
  .init = toyota_nolimits_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
};
