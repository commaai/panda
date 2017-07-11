#ifndef PANDA_SAFETY_H
#define PANDA_SAFETY_H

#include "can.h"

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired);

typedef void (*rx_hook)(CAN_FIFOMailBox_TypeDef *to_push);
typedef int (*tx_hook)(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
typedef int (*tx_lin_hook)(int lin_num, uint8_t *data, int len, int hardwired);

typedef struct {
  rx_hook rx;
  tx_hook tx;
  tx_lin_hook tx_lin;
} safety_hooks;

bool is_output_enabled();
int set_safety_mode(uint16_t mode);

extern int gas_interceptor_detected;

#endif
