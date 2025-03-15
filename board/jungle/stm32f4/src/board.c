#include "../board.h"

void detect_board_type(void) {
  hw_type = HW_TYPE_V1;
  current_board = &board_v1;
}
