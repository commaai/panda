#include "board.h"

#ifndef PANDA_JUNGLE
#error FUCK YOU
#endif

#include "board/globals.h"
#include "board/config.h"

extern board board_v2;

void detect_board_type(void) {
  hw_type = HW_TYPE_V2;
  current_board = &board_v2;
}
