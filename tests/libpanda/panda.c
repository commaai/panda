#include "board/config.h"
#include "board/drivers/drivers.h"
#include "board/comms.h"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
void can_tx_comms_resume_usb(void) { }
void can_tx_comms_resume_spi(void) { }

extern can_ring can_rx_q;
extern can_ring can_tx1_q;
extern can_ring can_tx2_q;
extern can_ring can_tx3_q;

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;
