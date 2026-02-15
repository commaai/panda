#pragma once

#include "board_declarations.h"

// ///////////////////////////// //
// Red Panda (STM32H7) + Harness //
// ///////////////////////////// //

void red_enable_can_transceiver(uint8_t transceiver, bool enabled);
void red_set_can_mode(uint8_t mode);
uint32_t red_read_voltage_mV(void);
void red_init(void);

extern harness_configuration red_harness_config;
extern board board_red;
