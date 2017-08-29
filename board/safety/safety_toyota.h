static void toyota_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

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
    if (controls_allowed) {
      // all messages are fine here
    } else {
      if (to_send->RDLR & 0xFFFF00) return 0;
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

