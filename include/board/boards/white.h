#pragma once

#include "boards/board.h"
#include "drivers/gpio.h"
// ///////////////////// //
// White Panda (STM32F4) //
// ///////////////////// //
void white_enable_can_transceiver(uint8_t transceiver, bool enabled);
void white_set_can_mode(uint8_t mode);
uint32_t white_read_voltage_mV(void);
uint32_t white_read_current_mA(void);
bool white_check_ignition(void);
void white_grey_init(void);
void white_grey_init_bootloader(void);
