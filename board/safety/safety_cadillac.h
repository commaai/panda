// Placeholder file, actual safety is TODO.
const safety_hooks cadillac_hooks = {
  .init = alloutput_init,
  .rx = default_rx_hook,
  .tx = alloutput_tx_hook,
  .tx_lin = alloutput_tx_lin_hook,
  .ignition = alloutput_ign_hook,
  .fwd = alloutput_fwd_hook,
};

