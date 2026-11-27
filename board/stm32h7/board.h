#ifndef BOARD_STM32H7_BOARD_H
#define BOARD_STM32H7_BOARD_H

// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "board/boards/board_declarations.h"
#include "board/boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "board/drivers/pwm.h"
#include "board/stm32h7/lladc.h"
#include "board/drivers/harness.h"
#include "board/stm32h7/sound.h"
#include "board/drivers/fake_siren.h"
#include "board/drivers/clock_source.h"
#include "board/boards/red.h"
#include "board/boards/tres.h"
#include "board/boards/cuatro.h"


// reads four consecutive ID pins, in order, as bits 0 to 3
static uint8_t detect_id_pins(GPIO_TypeDef *GPIO, unsigned int first_pin) {
  uint8_t id = 0U;
  for (unsigned int i = 0U; i < 4U; i++) {
    if (detect_with_pull(GPIO, first_pin + i, PULL_UP)) {
      id |= (uint8_t)(1U << i);
    }
  }
  return id;
}

static void detect_board_type(void) {
  // On STM32H7 pandas, we use two different sets of pins.
  const uint8_t id1 = detect_id_pins(GPIOF, 7U);
  const uint8_t id2 = detect_id_pins(GPIOD, 4U);

  if (id2 == 3U) {
    hw_type = HW_TYPE_CUATRO;
    current_board = &board_cuatro;
  } else if ((get_package_smps_type() == PACKAGE_WITH_SMPS) && (id1 == 0U)) {
    // id1 also reads 0 on cuatro, the package tells them apart
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
  }

  // check for non-supported board types
  assert_fatal(hw_type != HW_TYPE_UNKNOWN, "Unsupported board type\n");
}

#endif  // BOARD_STM32H7_BOARD_H
