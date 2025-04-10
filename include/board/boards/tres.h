// ///////////////////////////
// Tres (STM32H7) + Harness //
// ///////////////////////////
#pragma once
#include <stdbool.h>
#include <stdint.h>
extern bool tres_ir_enabled;
extern bool tres_fan_enabled;
void tres_set_ir_power(uint8_t percentage);
void tres_set_can_mode(uint8_t mode);
bool tres_read_som_gpio(void);
