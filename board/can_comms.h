#ifndef CAN_COMMS_H
#define CAN_COMMS_H

#include "board/config.h"

// cppcheck-suppress misra-c2012-2.3 ; used in driver implementations
// cppcheck-suppress misra-c2012-2.4 ; used in driver implementations
typedef struct {
  uint32_t ptr;
  uint32_t tail_size;
  uint8_t data[72];
} asm_buffer;

int comms_can_read(uint8_t *data, uint32_t max_len);
void comms_can_write(const uint8_t *data, uint32_t len);
void comms_can_reset(void);
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void);
void can_tx_comms_resume_spi(void);
uint8_t can_tx_slots_available(FDCAN_GlobalTypeDef *FDCANx);

#endif
