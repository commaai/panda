#include "board/body/motor_common.h"

bool body_motor_is_valid(uint8_t motor) {
  return (motor > 0U) && (motor <= BODY_MOTOR_COUNT);
}
