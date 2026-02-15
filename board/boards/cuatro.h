#pragma once

#include "board_declarations.h"

// ////////////////////////// //
// Cuatro (STM32H7) + Harness //
// ////////////////////////// //

void cuatro_enable_can_transceiver(uint8_t transceiver, bool enabled);
uint32_t cuatro_read_voltage_mV(void);
uint32_t cuatro_read_current_mA(void);
void cuatro_set_fan_enabled(bool enabled);
void cuatro_set_bootkick(BootState state);
void cuatro_set_amp_enabled(bool enabled);
void cuatro_init(void);

extern harness_configuration cuatro_harness_config;
extern board board_cuatro;
