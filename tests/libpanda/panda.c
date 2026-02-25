#include "fake_stm.h"
#include "config.h"
#include "can.h"
#include "drivers/registers.h"
#include "drivers/simple_watchdog.h"
#include "drivers/led.h"
#include "drivers/gpio.h"
#include "drivers/pwm.h"
#include "drivers/timers.h"
#include "drivers/bootkick.h"
#include "drivers/clock_source.h"
#include "drivers/fan.h"
#include "drivers/harness.h"
#include "drivers/spi.h"
#include "drivers/usb.h"
#include "drivers/can_common.h"

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
#include "main_globals.h"
#include "drivers/can_common.h"
#include "drivers/can_common.c"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "comms.h"
#include "can_comms.h"
#include "can_comms.c"
