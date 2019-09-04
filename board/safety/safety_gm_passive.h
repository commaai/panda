// All sending is disallowed.
// The only difference from "no output" model
// is using GM ignition hook.

static void gm_passive_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if ((addr == 0x1F1) && (bus_number == 0)) {
    bool ign = (GET_BYTE(to_push, 0) & 0x20) != 0;
    gm_ignition_started = ign;
  }
}

const safety_hooks gm_passive_hooks = {
  .init = gm_init,
  .rx = gm_passive_rx_hook,
  .tx = nooutput_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .ignition = gm_ign_hook,
  .fwd = default_fwd_hook,
};
