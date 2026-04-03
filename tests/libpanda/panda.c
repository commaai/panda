#include "fake_stm.h"
#include "config.h"
#include "can.h"

bool can_init(uint8_t can_number) { UNUSED(can_number); return true; }
void process_can(uint8_t can_number) { UNUSED(can_number); }

typedef struct harness_configuration harness_configuration;
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "sys/faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "opendbc/safety/declarations.h"
#include "main_declarations.h"
#include "drivers/can_common.h"
#include "can_comms.h"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

FDCAN_GlobalTypeDef *cans[PANDA_CAN_CNT] = {NULL, NULL, NULL};

void set_intercept_relay(bool intercept, bool ignition_relay) { UNUSED(intercept); UNUSED(ignition_relay); }
void can_clear_send(FDCAN_GlobalTypeDef *FDCANx, uint8_t can_number) { UNUSED(FDCANx); UNUSED(can_number); }
