uint16_t ego_speed = 0;

static void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  // get the up to date speed
  if ((to_push->RIR>>21) == 0xb4) {
    // not sure about the unit of this
    ego_speed = ((to_push->RDHR) & 0xFF00) | ((to_push->RDHR >> 16) & 0xFF);
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

    if (controls_allowed) {
      // all messages are fine here
      // TODO: add speed dependant torque limit
    } else {
      if (desired_torque != 0) return 0;
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

