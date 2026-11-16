#ifndef BOARD_MAIN_DEFINITIONS_H
#define BOARD_MAIN_DEFINITIONS_H

#include "board/drivers/drivers.h"

typedef struct board board;
typedef struct harness_configuration harness_configuration;

// ********************* Globals **********************
static uint8_t hw_type = 0;
static board *current_board;

#endif  // BOARD_MAIN_DEFINITIONS_H
