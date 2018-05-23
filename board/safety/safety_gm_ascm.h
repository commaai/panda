static int gm_ascm_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  if (bus_num == 0) {
    if (((to_fwd->RIR>>21) == 0x152) || ((to_fwd->RIR>>21) == 0x154)) {
      return -1;
    }
    return 2;
  }

  if (bus_num == 2) {
    if (((to_fwd->RIR>>21) == 0x152) || ((to_fwd->RIR>>21) == 0x154)) {
      return -1;
    }
    return 0;
  }
  return -1;
}

const safety_hooks gm_ascm_hooks = {
  .init = nooutput_init,
  .rx = default_rx_hook,
  .tx = alloutput_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = gm_ascm_fwd_hook,
};

