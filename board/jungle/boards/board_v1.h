// ///////////////////////// //
// Jungle board v1 (STM32F4) //
// ///////////////////////// //
#pragma once
#include <stdbool.h>
#include <stdint.h>

void board_v1_enable_can_transceiver(uint8_t transceiver, bool enabled);
void board_v1_set_can_mode(uint8_t mode);
void board_v1_set_harness_orientation(uint8_t orientation);
void board_v1_set_panda_power(bool enable);
bool board_v1_get_button(void);
void board_v1_set_ignition(bool enabled);
float board_v1_get_channel_power(uint8_t channel);
uint16_t board_v1_get_sbu_mV(uint8_t channel, uint8_t sbu);
void board_v1_init(void);
void board_v1_tick(void);
