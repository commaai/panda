#include "power_saving.h"
#include "libc.h"
#include "drivers/gpio.h"
#include "drivers/harness.h"
#include "boards/board.h"

#ifdef STM32H7
  #include "drivers/fdcan.h"
  #include "stm32h7/llfdcan.h"
#else
  #include "drivers/bxcan.h"
  #include "stm32f4/llbxcan.h"
#endif

// WARNING: To stay in compliance with the SIL2 rules laid out in STM UM1840, we should never implement any of the available hardware low power modes.
// See rule: CoU_3

int power_save_status = POWER_SAVE_STATUS_DISABLED;

void enable_can_transceivers(bool enabled) {
  // Leave main CAN always on for CAN-based ignition detection
  uint8_t main_bus = (current_board->harness_config->has_harness && (harness.status == HARNESS_STATUS_FLIPPED)) ? 3U : 1U;
  for(uint8_t i=1U; i<=4U; i++){
    current_board->enable_can_transceiver(i, (i == main_bus) || enabled);
  }
}

void set_power_save_state(int state) {
  bool is_valid_state = (state == POWER_SAVE_STATUS_ENABLED) || (state == POWER_SAVE_STATUS_DISABLED);
  if (is_valid_state && (state != power_save_status)) {
    bool enable = false;
    if (state == POWER_SAVE_STATUS_ENABLED) {
      print("enable power savings\n");

      // Disable CAN interrupts
      if (harness.status == HARNESS_STATUS_FLIPPED) {
        llcan_irq_disable(cans[0]);
      } else {
        llcan_irq_disable(cans[2]);
      }
      llcan_irq_disable(cans[1]);
    } else {
      print("disable power savings\n");

      if (harness.status == HARNESS_STATUS_FLIPPED) {
        llcan_irq_enable(cans[0]);
      } else {
        llcan_irq_enable(cans[2]);
      }
      llcan_irq_enable(cans[1]);

      enable = true;
    }

    enable_can_transceivers(enable);

    // Switch off IR when in power saving
    if(!enable){
      current_board->set_ir_power(0U);
    }

    power_save_status = state;
  }
}
