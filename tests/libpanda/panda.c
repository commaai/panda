// Test-specific definitions to avoid conflicts
#include <stdint.h>
#include <stdbool.h>
#include "board/config.h"
#include "board/can.h"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }

typedef struct harness_configuration harness_configuration;
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "board/health.h"
#include "board/faults.h"
#include "board/libc.h"
#include "board/boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "board/main_definitions.h"
#include "board/drivers/can_common.h"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "board/comms_definitions.h"
#include "board/can_comms.h"
