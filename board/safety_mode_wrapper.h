#ifndef SAFETY_MODE_WRAPPER_H
#define SAFETY_MODE_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

void set_safety_mode(uint16_t mode, uint32_t param);
void set_safety_param(uint32_t param);
bool is_car_safety_mode(uint16_t mode);

#endif
