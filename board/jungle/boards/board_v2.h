// ///////////////////////// //
// Jungle board v2 (STM32H7) //
// ///////////////////////// //

void board_v2_set_led(uint8_t color, bool enabled);

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

board board_v2 = {
  .has_canfd = true,
  .has_sbu_sense = true,
  .avdd_mV = 3300U,
  .init = &board_v2_init,
  .set_led = &board_v2_set_led,
  .board_tick = &board_v2_tick,
  .get_button = &board_v2_get_button,
  .set_panda_power = &board_v2_set_panda_power,
  .set_panda_individual_power = &board_v2_set_panda_individual_power,
  .set_ignition = &board_v2_set_ignition,
  .set_individual_ignition = &board_v2_set_individual_ignition,
  .set_harness_orientation = &board_v2_set_harness_orientation,
  .set_can_mode = &board_v2_set_can_mode,
  .enable_can_transceiver = &board_v2_enable_can_transceiver,
  .enable_header_pin = &board_v2_enable_header_pin,
  .get_channel_power = &board_v2_get_channel_power,
  .get_sbu_mV = &board_v2_get_sbu_mV,
};
