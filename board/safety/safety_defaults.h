void default_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {}

// *** no output safety mode ***

static void nooutput_init() {
}

static int nooutput_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired) {
  return false;
}

static int nooutput_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired) {
  return false;
}

const safety_hooks nooutput_hooks = {
  .init = nooutput_init,
  .rx = default_rx_hook,
  .tx = nooutput_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
};

// *** all output safety mode ***

static void alloutput_init() {
}

static int alloutput_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired) {
  return hardwired;
}

static int alloutput_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired) {
  return hardwired;
}

const safety_hooks alloutput_hooks = {
  .init = alloutput_init,
  .rx = default_rx_hook,
  .tx = alloutput_tx_hook,
  .tx_lin = alloutput_tx_lin_hook,
};

