#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BODY_MOTOR_COUNT 2U

typedef enum {
  BODY_MOTOR_LEFT = 1U,
  BODY_MOTOR_RIGHT = 2U,
} body_motor_id_e;

#define BODY_CAN_ADDR_TARGET_RPM       0x600U
#define BODY_CAN_ADDR_MOTOR_SPEED      0x201U
#define BODY_CAN_MOTOR_SPEED_PERIOD_US 10000U

static inline bool body_motor_is_valid(uint8_t motor) {
  return (motor > 0U) && (motor <= BODY_MOTOR_COUNT);
}
