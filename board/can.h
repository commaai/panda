#ifndef PANDA_CAN_H
#define PANDA_CAN_H

#include <stdbool.h>
#include "rev.h"

#define CAN_TIMEOUT 1000000
#define PANDA_CANB_RETURN_FLAG 0x80

extern bool can_live, pending_can_live, can_loopback;

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

extern can_ring can_rx_q;

// ********************* port description types *********************

#define CAN_PORT_DESC_INITIALIZER               \
  .forwarding=-1,                               \
  .bitrate=CAN_DEFAULT_BITRATE,                 \
  .bitrate=CAN_DEFAULT_BITRATE,                 \
  .gmlan=false,                                 \
  .gmlan_bitrate=GMLAN_DEFAULT_BITRATE

typedef struct {
  GPIO_TypeDef* port;
  uint8_t num;
  bool high_val;
} gpio_pin;

typedef struct {
  GPIO_TypeDef* port;
  uint8_t num;
  uint32_t setting;
} gpio_alt_setting;

typedef struct {
  CAN_TypeDef *CAN;
  int8_t forwarding;
  uint32_t bitrate;
  bool gmlan;
  bool gmlan_support;
  uint32_t gmlan_bitrate;
  gpio_pin enable_pin;
  gpio_alt_setting can_pins[2];
  gpio_alt_setting gmlan_pins[2];
  can_ring *msg_buff;
} can_port_desc;

extern can_port_desc can_ports[];

#ifdef PANDA
  #define CAN_MAX 3
#else
  #define CAN_MAX 2
#endif

// ********************* interrupt safe queue *********************

int pop(can_ring *q, CAN_FIFOMailBox_TypeDef *elem);

int push(can_ring *q, CAN_FIFOMailBox_TypeDef *elem);

// ********************* CAN Functions *********************

void can_init(uint8_t canid);

void set_can_mode(uint16_t can, bool use_gmlan);

void send_can(CAN_FIFOMailBox_TypeDef *to_push, uint8_t canid);

// CAN error
//void can_sce(uint8_t canid);

int can_cksum(uint8_t *dat, int len, int addr, int idx);

#endif
