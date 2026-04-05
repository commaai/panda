#ifndef MAIN_DECLARATIONS_H
#define MAIN_DECLARATIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "board/drivers/driver_declarations.h"

extern void __initialize_hardware_early(void);

extern uint8_t hw_type;
extern const board *current_board;
extern uint32_t uptime_cnt;
extern uint32_t enter_bootloader_mode;

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

// siren state
extern bool siren_enabled;
extern uint32_t siren_countdown;

#endif
