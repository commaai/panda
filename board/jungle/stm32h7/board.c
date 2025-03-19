#include "board.h"

void detect_board_type(void) {
  hw_type = HW_TYPE_V2;
  current_board = &board_v2;
}
