#include "board/body/motor_control.h"

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

  set_gpio_output_type(GPIOD, 9U, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_mode(GPIOD, 9U, MODE_OUTPUT);
  set_gpio_output(GPIOD, 9U, true);

  set_gpio_pullup(GPIOD, 8, PULL_UP);
  set_gpio_mode(GPIOD, 8, MODE_INPUT);
  SYSCFG->EXTICR[2] &= ~(SYSCFG_EXTICR3_EXTI8);
  SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI8_PD;
  EXTI->PR1 = (1U << 8);
  EXTI->RTSR1 |= (1U << 8);
  EXTI->FTSR1 |= (1U << 8);
  EXTI->IMR1 |= (1U << 8);

  set_gpio_pullup(GPIOE, 13, PULL_UP);
  set_gpio_mode(GPIOE, 13, MODE_INPUT);
  SYSCFG->EXTICR[3] &= ~(SYSCFG_EXTICR4_EXTI13);
  SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PE;
  EXTI->PR1 = (1U << 13);
  EXTI->RTSR1 |= (1U << 13);
  EXTI->FTSR1 |= (1U << 13);
  EXTI->IMR1 |= (1U << 13);
}

board board_body = {
  .led_GPIO = {GPIOC, GPIOC, GPIOC},
  .led_pin = {7, 7, 7},
  .init = board_body_init,
};
