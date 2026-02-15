#include "fake_stm.h"
#include "config.h"
#include "can.h"
#include "health.h"
#include "faults.h"
#include "libc.h"

typedef struct harness_configuration harness_configuration;
#include "boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "main_definitions.h"
#include "drivers/can_common.h"
#include "comms_definitions.h"
#include "can_comms.h"

// Necessary pointers for Python tests
can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

// Hardware/Platform mocks
bool can_init(uint8_t can_number) { (void)can_number; return true; }
void process_can(uint8_t can_number) { (void)can_number; }

// Driver implementation mocks
void can_tx_comms_resume_usb(void) { }
void can_tx_comms_resume_spi(void) { }

// Fake STM implementations
void print(const char *a) {
  printf("%s", a);
}

void puth(unsigned int i) {
  printf("%x", i);
}

TIM_TypeDef timer = { .CNT = 0 };
TIM_TypeDef *MICROSECOND_TIMER = &timer;

uint32_t microsecond_timer_get(void) {
  return MICROSECOND_TIMER->CNT;
}
