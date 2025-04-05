#include "fake_stm_panda.h"
#include "config.h"
#include "can_panda.h"
#include "drivers/harness_configuration.h"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
//int safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "faults_panda.h"
#include "libc.h"
#include "safety/safety.h"
#include "main_definitions.h"
#include "boards/board.h"
#include "drivers/can_common_panda.h"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "comms_definitions.h"
#include "can_comms.h"
