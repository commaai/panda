#pragma once

#include "board_declarations.h"

// ///////////////////////////
// Tres (STM32H7) + Harness //
// ///////////////////////////

extern bool tres_ir_enabled;
extern bool tres_fan_enabled;

void tres_update_fan_ir_power(void);
void tres_set_ir_power(uint8_t percentage);
void tres_set_bootkick(BootState state);
void tres_set_fan_enabled(bool enabled);
void tres_enable_can_transceiver(uint8_t transceiver, bool enabled);
void tres_set_can_mode(uint8_t mode);
bool tres_read_som_gpio (void);
void tres_init(void);

extern harness_configuration tres_harness_config;
extern board board_tres;
