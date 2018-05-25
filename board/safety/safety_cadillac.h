int cadillac_ignition_started = 0;

static void cadillac_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr = to_push->RIR >> 21;

  if (addr == 0x135 && bus_number == 0) {
    cadillac_ignition_started = 1; //as soona s we receive can msgs, ingition is on
  }
}

static void cadillac_init(int16_t param) {
  cadillac_ignition_started = 0;
}

static int cadillac_ign_hook() {
  return cadillac_ignition_started;
}

// Placeholder file, actual safety is TODO.
const safety_hooks cadillac_hooks = {
  .init = cadillac_init,
  .rx = cadillac_rx_hook,
  .tx = alloutput_tx_hook,
  .tx_lin = alloutput_tx_lin_hook,
  .ignition = cadillac_ign_hook,
  .fwd = alloutput_fwd_hook,
};
