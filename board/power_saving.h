#include <stdbool.h>

extern bool power_save_enabled;

void switch_power_mode(bool enable) {
  int set_state = enable;

  // toggle CAN
  set_can_enable(CAN1, set_state);
  set_can_enable(CAN2, set_state);
  set_can_enable(CAN3, set_state);

  // toggle GMLAN
  set_gpio_output(GPIOB, 14, set_state);
  set_gpio_output(GPIOB, 15, set_state);

  // toggle LIN
  set_gpio_output(GPIOB, 7, set_state);
  set_gpio_output(GPIOA, 14, set_state);

  if (is_grey_panda == true) {
    // wake
    char* UBLOX_POWER_STATE_MSG = "\xb5\x62\x06\x04\x04\x00\x01\x00\x09\x00\x18\x7a";
    if (enable) {
      // sleep
      UBLOX_POWER_STATE_MSG = "\xb5\x62\x06\x04\x04\x00\x01\x00\x08\x00\x17\x78";
    }
    uart_ring *ur = get_ring_by_number(1);
    for (int i = 0; i < (int)(sizeof(UBLOX_POWER_STATE_MSG))-1; i++) while (!putc(ur, UBLOX_POWER_STATE_MSG[i]));
  }
  power_save_enabled = enable;
}

void power_save_mode(bool enable) {
  if (enable && !power_save_enabled) {
    puts("enable power savings\n");
    switch_power_mode(enable);
  } else if (!enable && power_save_enabled) {
    puts("disable power savings\n");
    switch_power_mode(enable);
  } else {
    // already in requested mode
  }
}

