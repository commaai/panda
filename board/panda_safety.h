// pandas by default do not allow sending in this firmware

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
}

int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  return 0;
}

int safety_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  return 0;
}

