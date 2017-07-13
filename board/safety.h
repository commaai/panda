#ifndef PANDA_SAFETY_H
#define PANDA_SAFETY_H

#include "can.h"

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired);

#endif
