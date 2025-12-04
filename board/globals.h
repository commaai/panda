#pragma once

#include <stdint.h>

#include "boards/board_declarations.h"
#include "board/drivers/harness.h"

typedef struct board board;

extern uint8_t hw_type;
extern board *current_board;
extern struct harness_t harness;
extern uint32_t uptime_cnt;