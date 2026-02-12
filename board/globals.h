#pragma once

#include <stdint.h>

#include "board/boards/boards.h"
#include "board/drivers/drivers.h"

extern uint8_t hw_type;
extern struct board *current_board;
extern struct harness_t harness;
extern uint32_t uptime_cnt;
extern int _app_start[0xc000];

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;            // set over USB

 // siren state
extern bool siren_enabled;