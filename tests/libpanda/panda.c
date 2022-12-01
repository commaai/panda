#include "fake_stm.h"
#include "config.h"
#include "can_definitions.h"

bool bitbang_gmlan(CANPacket_t *to_bang) { return true; }
bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
//int safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void usb_cb_ep3_out_complete(void);
void usb_outep3_resume_if_paused(void) { };

#include "health.h"
#include "faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "safety.h"
#include "main_declarations.h"
#include "drivers/can_common.h"

can_ring *rx_q = &can_rx_q;
can_ring *txgmlan_q = &can_txgmlan_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "comms_definitions.h"
#include "can_comms.h"

// libpanda stuff
#include "safety_helpers.h"
