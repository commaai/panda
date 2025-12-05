#include "board/config.h"

#include "board/jungle/boards/board_declarations.h"

#include "board/stm32h7/lladc.h"
#include "board/board_struct.h"

extern struct board board_v2;

void detect_board_type(void);