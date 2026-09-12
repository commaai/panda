#include "fake_stm.h"
#include "config.h"
#include "can.h"

bool can_init(uint8_t can_number) { return true; }
uint32_t process_can_calls[PANDA_CAN_CNT] = {0U};
uint32_t process_can_batch_calls[PANDA_CAN_CNT] = {0U};
void process_can(uint8_t can_number) { process_can_calls[can_number] += 1U; }
uint32_t process_can_batch_budget[PANDA_CAN_CNT] = {0U};
void process_can_batch(uint8_t can_number, uint32_t max_packets) {
  process_can_batch_calls[can_number] += 1U;
  process_can_batch_budget[can_number] += max_packets;
}
//int safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "sys/faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "main_definitions.h"
#include "drivers/can_common.h"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "comms_definitions.h"
// Exercise the same packed unaligned word accessors as the firmware.
#include "stm32h7/inc/cmsis_gcc.h"
#include "can_comms.h"
