#pragma once

#include "board/drivers/drivers.h"

// ///////////////////////////// //
// Red Panda (STM32H7) + Harness //
// ///////////////////////////// //

extern struct harness_configuration red_harness_config;
extern struct board board_red;

uint32_t red_read_voltage_mV(void);