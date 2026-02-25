#pragma once

#include <stdint.h>

void motor_encoder_init(void);
void motor_encoder_reset(uint8_t motor);
int32_t motor_encoder_get_position(uint8_t motor);
float motor_encoder_get_speed_rpm(uint8_t motor);
