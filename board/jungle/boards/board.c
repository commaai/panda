#include "jungle/boards/board.h"
#include "utils.h"

uint8_t harness_orientation = HARNESS_ORIENTATION_NONE;
uint8_t can_mode = CAN_MODE_NORMAL;
uint8_t ignition = 0U;

void unused_set_individual_ignition(uint8_t bitmask) {
  UNUSED(bitmask);
}

void unused_board_enable_header_pin(uint8_t pin_num, bool enabled) {
  UNUSED(pin_num);
  UNUSED(enabled);
}

void unused_set_panda_individual_power(uint8_t port_num, bool enabled) {
  UNUSED(port_num);
  UNUSED(enabled);
}
