#pragma once

#include <stdint.h>

#include "board/board_struct.h"
#include "board/drivers/drivers.h"

extern uint8_t hw_type;
extern struct board *current_board;
extern struct harness_t harness;
extern uint32_t uptime_cnt;
extern int _app_start[0xc000];