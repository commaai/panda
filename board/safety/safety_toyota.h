// start with random large value (300kph) so torque is conservatively limited
// until a valid speed measurement is received
int32_t speed = 30000;
// 2 speed thresholds with 2 different steer torque levels allowed
const int32_t SPEED_0 = 1000;        // 5 kph + 5 kph margin VS controlsd
const int32_t SPEED_1 = 4500;       // 40 kph + 5 kph margin VS controlsd
const int32_t MAX_STEER_0 = 1500;   // max
const int32_t MAX_STEER_1 = 750;    // max/2

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

    if (controls_allowed) {
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

    } else if (desired_torque != 0) {
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
}

const safety_hooks toyota_hooks = {
  .init = toyota_init,
  .rx = toyota_rx_hook,
  .tx = toyota_tx_hook,
  .tx_lin = toyota_tx_lin_hook,
};

