#pragma once

#include <stdbool.h>
#include <stdint.h>

void motor_init(void);
void motor_speed_controller_init(void);
void motor_set_speed(uint8_t motor, int8_t speed);
void motor_speed_controller_set_target_rpm(uint8_t motor, float target_rpm);
void motor_speed_controller_update(uint32_t now_us);
