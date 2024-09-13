#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board_declarations.h"

// ////////////////////////// //
// Cuatro (STM32H7) + Harness //
// ////////////////////////// //

void cuatro_set_led(uint8_t color, bool enabled);
void cuatro_enable_can_transceiver(uint8_t transceiver, bool enabled);
void cuatro_enable_can_transceivers(bool enabled);
uint32_t cuatro_read_voltage_mV(void);
uint32_t cuatro_read_current_mA(void);
void cuatro_set_fan_enabled(bool enabled);
void cuatro_set_bootkick(BootState state);
void cuatro_init(void);

extern board board_cuatro;
