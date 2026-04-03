#ifndef PANDA_BODY_STM32H7_BOARD_H
#define PANDA_BODY_STM32H7_BOARD_H

#include "board/body/boards/board_declarations.h"
#include "board/body/boards/board_body.h"

extern const board *current_board;
extern uint8_t hw_type;

void detect_board_type(void);

#endif
