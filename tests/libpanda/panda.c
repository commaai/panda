#include "fake_stm.h"
#include "config.h"
#include "can.h"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
//int safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "sys/faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "main_definitions.h"
#include "drivers/can_common.h"

// CAN queue definitions for libpanda test build
// (in the firmware these are defined in board/drivers/can_common.c)
#define CAN_RX_BUFFER_SIZE 4096U
#define CAN_TX_BUFFER_SIZE 416U
static CANPacket_t elems_rx_q[CAN_RX_BUFFER_SIZE];
can_ring can_rx_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_RX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_rx_q };
static CANPacket_t elems_tx1_q[CAN_TX_BUFFER_SIZE];
can_ring can_tx1_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_TX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_tx1_q };
static CANPacket_t elems_tx2_q[CAN_TX_BUFFER_SIZE];
can_ring can_tx2_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_TX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_tx2_q };
static CANPacket_t elems_tx3_q[CAN_TX_BUFFER_SIZE];
can_ring can_tx3_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_TX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_tx3_q };

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "comms_definitions.h"
#include "can_comms.h"
