static int elm327_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  int tx = 1;

  //All ELM traffic must appear on CAN0
  if (((to_send->RDTR >> 4) & 0xf) != 0) {
    tx = 0;
  }

  //All ISO 15765-4 messages must be 8 bytes long
  if ((to_send->RDTR & 0xf) != 8) {
    tx = 0;
  }

  if (to_send->RIR & 4) {
    uint32_t addr = to_send->RIR >> 3;
    //Check valid 29 bit send addresses for ISO 15765-4
    if (!((addr == 0x18DB33F1) || ((addr & 0x1FFF00FF) == 0x18DA00F1))) {
      tx = 0;
    }
  } else {
    uint32_t addr = to_send->RIR >> 21;
    //Check valid 11 bit send addresses for ISO 15765-4
    if (!((addr == 0x7DF) || ((addr & 0x7F8) == 0x7E0))) {
      tx = 0;
    }
  }
  return tx;
}

static int elm327_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  int tx = 1;
  if (lin_num != 0) {
    tx = 0;  //Only operate on LIN 0, aka serial 2
  }
  if ((len < 5) || (len > 11)) {
    tx = 0;  //Valid KWP size
  }
  if (!(((data[0] & 0xF8) == 0xC0) && ((data[0] & 0x07) > 0) &&
        (data[1] == 0x33) && (data[2] == 0xF1))) {
    tx = 0;  //Bad msg
  }
  return tx;
}

const safety_hooks elm327_hooks = {
  .init = nooutput_init,
  .rx = default_rx_hook,
  .tx = elm327_tx_hook,
  .tx_lin = elm327_tx_lin_hook,
  .ignition = default_ign_hook,
  .fwd = default_fwd_hook,
};
