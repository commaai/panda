#pragma once

#include <stdint.h>
#include <stdbool.h>

// ///////////////////////////
// Tres (STM32H7) + Harness //
// ///////////////////////////

void tres_update_fan_ir_power(void);
void tres_set_ir_power(uint8_t percentage);
void tres_set_bootkick(BootState state);
void tres_set_fan_enabled(bool enabled);
bool tres_read_som_gpio (void);
void tres_init(void);

extern board board_tres;
