// ///////////////////////// //
// Jungle board v2 (STM32H7) //
// ///////////////////////// //
#pragma once
#include <stdbool.h>
#include <stdint.h>

void board_v2_set_harness_orientation(uint8_t orientation);
void board_v2_enable_can_transceiver(uint8_t transceiver, bool enabled);
void board_v2_enable_header_pin(uint8_t pin_num, bool enabled);
void board_v2_set_can_mode(uint8_t mode);
void board_v2_set_panda_power(bool enable);
void board_v2_set_panda_individual_power(uint8_t port_num, bool enable);
bool board_v2_get_button(void);
void board_v2_set_ignition(bool enabled);
void board_v2_set_individual_ignition(uint8_t bitmask);
float board_v2_get_channel_power(uint8_t channel);
uint16_t board_v2_get_sbu_mV(uint8_t channel, uint8_t sbu);
void board_v2_init(void);
void board_v2_tick(void);
