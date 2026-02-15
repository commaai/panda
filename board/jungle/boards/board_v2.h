#pragma once

// ///////////////////////// //
// Jungle board v2 (STM32H7) //
// ///////////////////////// //

#define ADC_CHANNEL(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_810_CYCLES, .oversampling = OVERSAMPLING_1}

extern gpio_t power_pins[];
extern gpio_t sbu1_ignition_pins[];
extern gpio_t sbu1_relay_pins[];
extern gpio_t sbu2_ignition_pins[];
extern gpio_t sbu2_relay_pins[];

extern const adc_signal_t sbu1_channels[];
extern const adc_signal_t sbu2_channels[];

void board_v2_set_harness_orientation(uint8_t orientation);
void board_v2_enable_can_transceiver(uint8_t transceiver, bool enabled);
void board_v2_enable_header_pin(uint8_t pin_num, bool enabled);
void board_v2_set_can_mode(uint8_t mode);

extern bool panda_power;
extern uint8_t panda_power_bitmask;
void board_v2_set_panda_power(bool enable);
void board_v2_set_panda_individual_power(uint8_t port_num, bool enable);
bool board_v2_get_button(void);
void board_v2_set_ignition(bool enabled);
void board_v2_set_individual_ignition(uint8_t bitmask);
float board_v2_get_channel_power(uint8_t channel);
uint16_t board_v2_get_sbu_mV(uint8_t channel, uint8_t sbu);
void board_v2_init(void);
void board_v2_tick(void);

extern board board_v2;
