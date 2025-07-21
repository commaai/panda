#include "led.h"

#ifdef STM32H7
  #include "stm32h7xx.h"
  #include "stm32h7xx_hal_gpio_ex.h"
#elif defined(STM32F4)
  #include "stm32f4xx.h"
  #include "stm32f4xx_hal_gpio_ex.h"
#endif

#include "board/drivers/pwm.h"
#include "board/drivers/gpio.h"
#include "board/main_declarations.h"

#ifndef BOOTSTUB
#include "board/boards/board_declarations.h"
#endif

void led_set(uint8_t color, bool enabled) {
#ifndef BOOTSTUB
  if (color < 3U) {
    if (current_board->led_pwm_channels[color] != 0U) {
      pwm_set(TIM3, current_board->led_pwm_channels[color], 100U - (enabled ? LED_PWM_POWER : 0U));
    } else {
      set_gpio_output(current_board->led_GPIO[color], current_board->led_pin[color], !enabled);
    }
  }
#else
  (void)color;
  (void)enabled;
#endif
}

void led_init(void) {
#ifndef BOOTSTUB
  for (uint8_t i = 0U; i<3U; i++){
    set_gpio_pullup(current_board->led_GPIO[i], current_board->led_pin[i], PULL_NONE);
    set_gpio_output_type(current_board->led_GPIO[i], current_board->led_pin[i], OUTPUT_TYPE_OPEN_DRAIN);

    if (current_board->led_pwm_channels[i] != 0U) {
      set_gpio_alternate(current_board->led_GPIO[i], current_board->led_pin[i], GPIO_AF2_TIM3);
      pwm_init(TIM3, current_board->led_pwm_channels[i]);
    } else {
      set_gpio_mode(current_board->led_GPIO[i], current_board->led_pin[i], MODE_OUTPUT);
    }

    led_set(i, false);
  }
#endif
}