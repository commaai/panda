// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/board_declarations.h"
#include "boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "stm32h7/lladc.h"
#include "drivers/harness.h"
#include "drivers/fan.h"
#include "stm32h7/llfan.h"
#include "stm32h7/lldac.h"
#include "drivers/fake_siren.h"
#include "drivers/clock_source.h"
#include "boards/red.h"
#include "boards/red_chiplet.h"
#include "boards/tres.h"
#include "boards/cuatro.h"


void detect_board_type(void) {
  /*
    On STM32H7-based pandas, we use two different sets of pins.
    ID2 is only used on certain packages, detected using the PKGR register.
  */
  const uint8_t id1 = detect_with_pull(GPIOF, 7, PULL_UP) |
                     (detect_with_pull(GPIOF, 8, PULL_UP) << 1U) |
                     (detect_with_pull(GPIOF, 9, PULL_UP) << 2U) |
                     (detect_with_pull(GPIOF, 10, PULL_UP) << 3U);

  const uint8_t id2 = detect_with_pull(GPIOD, 4, PULL_UP) |
                     (detect_with_pull(GPIOD, 5, PULL_UP) << 1U) |
                     (detect_with_pull(GPIOD, 6, PULL_UP) << 2U) |
                     (detect_with_pull(GPIOD, 7, PULL_UP) << 3U);

  const uint8_t board_id = (SYSCFG->PKGR == 1U) ? id2 : id1;
  if (board_id == 0U) {
    hw_type = HW_TYPE_RED_PANDA;
    current_board = &board_red;
  } else if (board_id == 1U) {
    // deprecated
    //hw_type = HW_TYPE_RED_PANDA_V2;
    hw_type = HW_TYPE_UNKNOWN;
  } else if (board_id == 2U) {
    hw_type = HW_TYPE_TRES;
    current_board = &board_tres;
  } else if (board_id == 3U) {
    hw_type = HW_TYPE_CUATRO;
    current_board = &board_cuatro;
  } else {
    hw_type = HW_TYPE_UNKNOWN;
    print("Hardware type is UNKNOWN!\n");
  }
}
