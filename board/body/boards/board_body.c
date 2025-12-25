#include "board_body.h"

#include "board/config.h"
#include "board/board_struct.h"

void board_body_init(void) {
  motor_init();
  motor_encoder_init();
  motor_speed_controller_init();
  motor_encoder_reset(1);
  motor_encoder_reset(2);

  // Initialize CAN pins
  set_gpio_pullup(GPIOD, 0, PULL_NONE);
  set_gpio_alternate(GPIOD, 0, GPIO_AF9_FDCAN1);
  set_gpio_pullup(GPIOD, 1, PULL_NONE);
  set_gpio_alternate(GPIOD, 1, GPIO_AF9_FDCAN1);
}

struct board board_body = {
  .led_GPIO = {GPIOC, GPIOC, GPIOC},
  .led_pin = {7, 7, 7},
  .init = board_body_init,
};
