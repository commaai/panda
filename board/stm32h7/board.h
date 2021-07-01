// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "stm32h7/peripherals.h"
#include "../boards/board_declarations.h"

// ///// Board definition and detection ///// //
#include "drivers/harness.h"
#include "boards/red.h"

void detect_board_type(void) {
  if(!detect_with_pull(GPIOF, 7, PULL_UP) && !detect_with_pull(GPIOF, 8, PULL_UP) && !detect_with_pull(GPIOF, 9, PULL_UP) && !detect_with_pull(GPIOF, 10, PULL_UP)){
    //hw_type = HW_TYPE_RED_PANDA; //REDEBUG
    hw_type = HW_TYPE_DOS;
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
