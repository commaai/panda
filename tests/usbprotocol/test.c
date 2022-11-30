#include "fake_stm.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"


#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

#include "can_definitions.h"

bool bitbang_gmlan(CANPacket_t *to_bang) { return true; }
bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
int safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void usb_cb_ep3_out_complete(void);
void usb_outep3_resume_if_paused(void) { };

#include "config.h"
#include "health.h"
#include "libc.h"
#include "boards/board_declarations.h"
/*
#include "health.h"
#include "faults.h"
#include "main_declarations.h"
#include "power_saving.h"
*/

#include "safety_declarations.h"
#include "main_declarations.h"
#include "drivers/can_common.h"

can_ring *rx_q = &can_rx_q;
can_ring *txgmlan_q = &can_txgmlan_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#define POWER_SAVE_STATUS_DISABLED 0
#define POWER_SAVE_STATUS_ENABLED 1

#include "comms_definitions.h"
#include "can_comms.h"
