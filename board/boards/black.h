#pragma once

#include <stdint.h>
#include <stdbool.h>

// /////////////////////////////// //
// Black Panda (STM32F4) + Harness //
// /////////////////////////////// //

void black_enable_can_transceiver(uint8_t transceiver, bool enabled);
void black_enable_can_transceivers(bool enabled);
void black_set_led(uint8_t color, bool enabled);
void black_set_usb_load_switch(bool enabled);
void black_set_can_mode(uint8_t mode);
bool black_check_ignition(void);
void black_init(void);
void black_init_bootloader(void);

extern harness_configuration black_harness_config;
extern board board_black;
