// pandas by default do not allow sending in this firmware

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
}

int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired) {
  return hardwired && controls_allowed;
}

int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired) {
  return hardwired && controls_allowed;
}

