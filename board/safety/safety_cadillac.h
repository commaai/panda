const int CADILLAC_IGNITION_TIMEOUT = 1000000; // 1s
int cadillac_can_seen = 0;
uint32_t cadillac_ts_last = 0;

static void cadillac_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr = to_push->RIR >> 21;

  if (addr == 0x135 && bus_number == 0) {
    cadillac_can_seen = 1;
    cadillac_ts_last = TIM2->CNT; // reset timer when gear msg is received
  }
}

static void cadillac_init(int16_t param) {
  cadillac_can_seen = 0;
}

static int cadillac_ign_hook() {
  uint32_t ts = TIM2->CNT;
  uint32_t ts_elapsed = get_ts_elapsed(ts, cadillac_ts_last);
  if ((ts_elapsed > CADILLAC_IGNITION_TIMEOUT) || (!cadillac_can_seen)) {
    cadillac_can_seen = 0;
    return 0;
  }
  return 1;
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
