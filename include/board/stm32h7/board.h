// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#pragma once
#include "boards/board.h"
#include "boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "stm32h7/lladc.h"
#include "drivers/harness.h"
#include "drivers/fan.h"
#include "drivers/gpio.h"
#include "stm32h7/llfan.h"
#include "stm32h7/lldac.h"
#include "drivers/fake_siren.h"
#include "stm32h7/sound.h"
#include "drivers/clock_source.h"

extern uint8_t hw_type;
extern struct board *current_board;
extern void print(const char *a);

static inline void detect_board_type(void) {
  // On STM32H7 pandas, we use two different sets of pins.
  const uint8_t id1 = detect_with_pull(GPIOF, 7, PULL_UP) |
                     (detect_with_pull(GPIOF, 8, PULL_UP) << 1U) |
                     (detect_with_pull(GPIOF, 9, PULL_UP) << 2U) |
                     (detect_with_pull(GPIOF, 10, PULL_UP) << 3U);

  const uint8_t id2 = detect_with_pull(GPIOD, 4, PULL_UP) |
                     (detect_with_pull(GPIOD, 5, PULL_UP) << 1U) |
                     (detect_with_pull(GPIOD, 6, PULL_UP) << 2U) |
                     (detect_with_pull(GPIOD, 7, PULL_UP) << 3U);

  if (id2 == 3U) {
    hw_type = HW_TYPE_CUATRO;
    current_board = &board_cuatro;
  } else if (id1 == 0U) {
    hw_type = HW_TYPE_RED_PANDA;
    current_board = &board_red;
  } else if (id1 == 1U) {
    // deprecated
    //hw_type = HW_TYPE_RED_PANDA_V2;
    hw_type = HW_TYPE_UNKNOWN;
  } else if (id1 == 2U) {
    hw_type = HW_TYPE_TRES;
    current_board = &board_tres;
  } else {
    hw_type = HW_TYPE_UNKNOWN;
    print("Hardware type is UNKNOWN!\n");
  }
}
