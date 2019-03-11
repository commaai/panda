void subaru_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {}

// FIXME
// *** all output safety mode ***

static void subaru_init(int16_t param) {
  controls_allowed = 1;
  #ifdef PANDA
    set_lline_output(0); //Default to off
    lline_relay_init();
  #endif
}

static int subaru_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  return true;
}

static int subaru_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {

  // shifts bits 29 > 11
  int32_t addr = to_fwd->RIR >> 21;

  // forward CAN 0 > 1
  if (bus_num == 0) {
    return 2; // ES CAN
  }
  // forward CAN 1 > 0, except ES_LKAS
  else if (bus_num == 2) {

    // outback 2015
    if (addr == 0x164) {
      return -1;
    }
    // global platform
    if (addr == 0x122) {
      return -1;
    }

    return 0; // Main CAN
  }

  // fallback to do not forward
  return -1;
}

const safety_hooks subaru_hooks = {
  .init = subaru_init,
  .rx = subaru_rx_hook,
  .tx = subaru_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .ignition = default_ign_hook,
  .fwd = subaru_fwd_hook,
  .relay = alloutput_relay_hook,
};
