#pragma once

#include <stdbool.h>
#include <stdint.h>

// Prototypes
void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);
