const int STEER_MAX = 150; // 1s
const int CADILLAC_IGNITION_TIMEOUT = 1000000; // 1s

int cadillac_ign = 0;
int cadillac_cruise_engaged_last = 0;
uint32_t cadillac_ts_last = 0;

static void cadillac_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr = to_push->RIR >> 21;

  // this message isn't all zeros when ignition is on
  if ((addr == 0x160) && (bus_number == 0) && to_push->RDLR) {
    cadillac_ign = 1;
    cadillac_ts_last = TIM2->CNT; // reset timer when ign is received
  }

  // enter controls on rising edge of ACC, exit controls on ACC off
  if ((addr == 0x370) && (bus_number == 0)) {
    int cruise_engaged = to_push->RDLR & 0x800000;  // bit 23
    if (cruise_engaged && !cadillac_cruise_engaged_last) {
      controls_allowed = 1;
    } else if (!cruise_engaged) {
      controls_allowed = 0;
    }
    cadillac_cruise_engaged_last = cruise_engaged;
  }
}

static int cadillac_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  uint32_t addr = to_send->RIR >> 21;

  // block steering cmd above 150
  if (addr == 0x151 || addr == 0x152 || addr == 0x153 || addr == 0x154) {
    int lkas_cmd = ((to_send->RDLR & 0x3f) << 8) + ((to_send->RDLR & 0xff00) >> 8);
    lkas_cmd = to_signed(lkas_cmd, 14);
    // block message is controls are allowed and lkas command exceeds max, or
    // if controls aren't allowed and lkas cmd isn't 0
    if (controls_allowed && ((lkas_cmd > STEER_MAX) || (lkas_cmd < -STEER_MAX))) {
      return 0;
    } else if (!controls_allowed && lkas_cmd) return 0;
  }
  return true;
}

static void cadillac_init(int16_t param) {
  cadillac_ign = 0;
}

static int cadillac_ign_hook() {
  uint32_t ts = TIM2->CNT;
  uint32_t ts_elapsed = get_ts_elapsed(ts, cadillac_ts_last);
  if (ts_elapsed > CADILLAC_IGNITION_TIMEOUT) {
    cadillac_ign = 0;
  }
  return cadillac_ign;
}

// Placeholder file, actual safety is TODO.
const safety_hooks cadillac_hooks = {
  .init = cadillac_init,
  .rx = cadillac_rx_hook,
  .tx = cadillac_tx_hook,
  .tx_lin = alloutput_tx_lin_hook,
  .ignition = cadillac_ign_hook,
  .fwd = alloutput_fwd_hook,
};
