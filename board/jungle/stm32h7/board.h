#include "board/drivers/drivers.h"
#include "board/stm32h7/stm32h7.h"
#include "board/boards/boards.h"

#include "board/stm32h7/lladc.h"
#include "board/jungle/boards/board_v2.h"

void detect_board_type(void) {
  hw_type = HW_TYPE_V2;
  current_board = &board_v2;
}
