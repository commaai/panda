#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board_declarations.h"

// /////////////////////// //
// Dos (STM32F4) + Harness //
// /////////////////////// //

void dos_enable_can_transceiver(uint8_t transceiver, bool enabled);
void dos_enable_can_transceivers(bool enabled);
void dos_set_led(uint8_t color, bool enabled);
void dos_set_bootkick(BootState state);
void dos_set_can_mode(uint8_t mode);
bool dos_check_ignition(void);
void dos_set_ir_power(uint8_t percentage);
void dos_set_fan_enabled(bool enabled);
void dos_set_siren(bool enabled);
bool dos_read_som_gpio (void);
void dos_init(void);

extern harness_configuration dos_harness_config;
extern board board_dos;
