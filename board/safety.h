void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired);

typedef void (*safety_hook_init)();
typedef void (*rx_hook)(CAN_FIFOMailBox_TypeDef *to_push);
typedef int (*tx_hook)(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
typedef int (*tx_lin_hook)(int lin_num, uint8_t *data, int len, int hardwired);

typedef struct {
  safety_hook_init init;
  rx_hook rx;
  tx_hook tx;
  tx_lin_hook tx_lin;
} safety_hooks;

// Include the actual safety policies.
#include "safety_honda.h"

void default__rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {}

void nooutput__init() {
  controls_allowed = false;
}

int nooutput__tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired) {
  return false;
}

int nooutput__tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired) {
  return false;
}

void alloutput__init() {
  controls_allowed = true;
}

int alloutput__tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired) {
  return hardwired;
}

int alloutput__tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired) {
  return hardwired;
}

const safety_hooks nooutput_hooks = {
  .init = nooutput__init,
  .rx = default__rx_hook,
  .tx = alloutput__tx_hook,
  .tx_lin = alloutput__tx_lin_hook,
};

const safety_hooks alloutput_hooks = {
  .init = alloutput__init,
  .rx = default__rx_hook,
  .tx = alloutput__tx_hook,
  .tx_lin = alloutput__tx_lin_hook,
};

const safety_hooks *current_hooks = &nooutput_hooks;

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push){
  current_hooks->rx(to_push);
}

int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired){
  return current_hooks->tx(to_send, hardwired);
}

int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired){
  return current_hooks->tx_lin(lin_num, data, len, hardwired);
}


typedef struct {
  uint16_t id;
  const safety_hooks *hooks;
} safety_hook_config;

const safety_hook_config safety_hook_registry[] = {
  {0x0000, &nooutput_hooks},
  {0x0001, &honda_hooks},
  {0x1337, &alloutput_hooks},
};

#define HOOK_CONFIG_COUNT (sizeof(safety_hook_registry)/sizeof(safety_hook_config))

int set_safety_mode(uint16_t mode){
  int i;
  for(i = 0; i < HOOK_CONFIG_COUNT; i++){
    if(safety_hook_registry[i].id == mode){
      current_hooks = safety_hook_registry[i].hooks;
      current_hooks->init();
      return 0;
    }
  }

  return -1;
}
