void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len);

typedef void (*safety_hook_init)();
typedef void (*rx_hook)(CAN_FIFOMailBox_TypeDef *to_push);
typedef int (*tx_hook)(CAN_FIFOMailBox_TypeDef *to_send);
typedef int (*tx_lin_hook)(int lin_num, uint8_t *data, int len);

typedef struct {
  safety_hook_init init;
  rx_hook rx;
  tx_hook tx;
  tx_lin_hook tx_lin;
} safety_hooks;

// Include the actual safety policies.
#include "safety/safety_defaults.h"
#include "safety/safety_honda.h"

const safety_hooks *current_hooks = &nooutput_hooks;

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push){
  current_hooks->rx(to_push);
}

int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  return current_hooks->tx(to_send);
}

int safety_tx_lin_hook(int lin_num, uint8_t *data, int len){
  return current_hooks->tx_lin(lin_num, data, len);
}

typedef struct {
  uint16_t id;
  const safety_hooks *hooks;
} safety_hook_config;

#define SAFETY_NOOUTPUT 0
#define SAFETY_HONDA 1
#define SAFETY_ALLOUTPUT 0x1337

const safety_hook_config safety_hook_registry[] = {
  {SAFETY_NOOUTPUT, &nooutput_hooks},
  {SAFETY_HONDA, &honda_hooks},
  {SAFETY_ALLOUTPUT, &alloutput_hooks},
};

#define HOOK_CONFIG_COUNT (sizeof(safety_hook_registry)/sizeof(safety_hook_config))

int set_safety_mode(uint16_t mode){
  can_set_silent(mode == SAFETY_NOOUTPUT);
  for (int i = 0; i < HOOK_CONFIG_COUNT; i++) {
    if (safety_hook_registry[i].id == mode) {
      current_hooks = safety_hook_registry[i].hooks;
      current_hooks->init();
      return 0;
    }
  }
  return -1;
}

