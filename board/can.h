#ifndef PANDA_CAN_H
#define PANDA_CAN_H

#include <stdbool.h>
#define CAN_TIMEOUT 1000000

typedef struct {
  GPIO_TypeDef* port;
  uint8_t num;
  bool high_val;
} gpio_pin;

typedef struct {
  CAN_TypeDef *CAN;
  int8_t forwarding;
  uint32_t bitrate;
  bool gmlan;
  bool gmlan_support;
  uint8_t safety_mode;
  gpio_pin pin;
} can_port_desc;

extern can_port_desc can_ports[];

#ifdef PANDA
  #define CAN_MAX 3
#else
  #define CAN_MAX 2
#endif

// ********************* queues types *********************

typedef struct {
  uint32_t w_ptr;
  uint32_t r_ptr;
  uint32_t fifo_size;
  CAN_FIFOMailBox_TypeDef *elems;
} can_ring;

#define can_buffer(x, size) \
  CAN_FIFOMailBox_TypeDef elems_##x[size]; \
  can_ring can_##x = { .w_ptr = 0, .r_ptr = 0, .fifo_size = size, .elems = (CAN_FIFOMailBox_TypeDef *)&elems_##x };

// ********************* interrupt safe queue *********************

int pop(can_ring *q, CAN_FIFOMailBox_TypeDef *elem);

int push(can_ring *q, CAN_FIFOMailBox_TypeDef *elem);

// ********************* CAN Functions *********************

void can_init(uint8_t canid);

// CAN error
void can_sce(CAN_TypeDef *CAN);

int can_cksum(uint8_t *dat, int len, int addr, int idx);

extern int controls_allowed;

#endif
