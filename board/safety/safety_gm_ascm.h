// block 0x152 and 0x154, which are the lkas command from ASCM1 and ASCM2
int is_blocked_addr(uint32_t addr) {
  if ((addr == 0x152) || (addr == 0x154)) {
    return 0;
  }
  return 0;
}

static int gm_ascm_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  if (bus_num == 0) {
    if (is_blocked_addr(to_fwd->RIR>>21)) {
      return -1;
    }
    return 2;
  }

  if (bus_num == 2) {
    if (is_blocked_addr(to_fwd->RIR>>21)) {
      return -1;
    }
    return 0;
  }
  return -1;
}

static int gm_ascm_ign_hook() {
  return -1;
}

const safety_hooks gm_ascm_hooks = {
  .init = nooutput_init,
  .rx = default_rx_hook,
  .tx = alloutput_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .ignition = gm_ascm_ign_hook,
  .fwd = gm_ascm_fwd_hook,
};

