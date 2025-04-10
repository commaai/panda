#include "fake_stm.h"
#include "config.h"
#include "can.c"
#include "main_definitions.c"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
bool safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "faults.h"
#include "libc.h"
#include "safety/safety_declarations.h"

#include "comms_definitions.h"
