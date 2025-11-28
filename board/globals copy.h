#pragma once

#include <stdint.h>

#include "boards/board_declarations.h"

typedef struct board board;

extern uint8_t hw_type;
extern board *current_board;
