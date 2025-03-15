// ///////////////////////// //
// Jungle board v1 (STM32F4) //
// ///////////////////////// //

void board_v1_set_led(uint8_t color, bool enabled);

void board_v1_enable_can_transceiver(uint8_t transceiver, bool enabled);

void board_v1_set_can_mode(uint8_t mode);

void board_v1_set_harness_orientation(uint8_t orientation);

extern bool panda_power;
void board_v1_set_panda_power(bool enable);

bool board_v1_get_button(void);

void board_v1_set_ignition(bool enabled);

float board_v1_get_channel_power(uint8_t channel);

uint16_t board_v1_get_sbu_mV(uint8_t channel, uint8_t sbu);

void board_v1_init(void);

void board_v1_tick(void);

board board_v1 = {
  .has_canfd = false,
  .has_sbu_sense = false,
  .avdd_mV = 3300U,
  .init = &board_v1_init,
  .set_led = &board_v1_set_led,
  .board_tick = &board_v1_tick,
  .get_button = &board_v1_get_button,
  .set_panda_power = &board_v1_set_panda_power,
  .set_panda_individual_power = &unused_set_panda_individual_power,
  .set_ignition = &board_v1_set_ignition,
  .set_individual_ignition = &unused_set_individual_ignition,
  .set_harness_orientation = &board_v1_set_harness_orientation,
  .set_can_mode = &board_v1_set_can_mode,
  .enable_can_transceiver = &board_v1_enable_can_transceiver,
  .enable_header_pin = &unused_board_enable_header_pin,
  .get_channel_power = &board_v1_get_channel_power,
  .get_sbu_mV = &board_v1_get_sbu_mV,
};
