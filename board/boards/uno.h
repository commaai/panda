#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board_declarations.h"

// /////////////////////// //
// Uno (STM32F4) + Harness //
// /////////////////////// //

void uno_enable_can_transceiver(uint8_t transceiver, bool enabled);
void uno_enable_can_transceivers(bool enabled);
void uno_set_led(uint8_t color, bool enabled);
void uno_set_bootkick(BootState state);
void uno_set_can_mode(uint8_t mode);
bool uno_check_ignition(void);
void uno_set_usb_switch(bool phone);
void uno_set_ir_power(uint8_t percentage);
void uno_set_fan_enabled(bool enabled);
void uno_init(void);
void uno_init_bootloader(void);

extern harness_configuration uno_harness_config;
extern board board_uno;
