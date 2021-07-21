// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/board_declarations.h"

// ///// Board definition and detection ///// //
#include "drivers/harness.h"
#include "drivers/fan.h"
#include "stm32h7/llfan.h"
#include "stm32h7/llrtc.h"
#include "drivers/rtc.h"
#include "boards/red.h"

uint8_t board_id(void) {
  return ((detect_with_pull(GPIOF, 7, PULL_UP) << 3) | (detect_with_pull(GPIOF, 7, PULL_UP) << 2) | (detect_with_pull(GPIOF, 7, PULL_UP) << 1) | detect_with_pull(GPIOF, 7, PULL_UP));
}

void detect_board_type(void) {
  if(board_id() == 0){
    hw_type = HW_TYPE_RED_PANDA;
    current_board = &board_red;
  } else {
    hw_type = HW_TYPE_UNKNOWN;
    puts("Hardware type is UNKNOWN!\n");
  }
}

bool has_external_debug_serial = 0;

// FIXME: enable uart on external pads
void detect_external_debug_serial(void) {
  // detect if external serial debugging is present
  //has_external_debug_serial = detect_with_pull(GPIOA, 3, PULL_DOWN);
}
