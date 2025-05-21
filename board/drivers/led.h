

#define LED_RED 0U
#define LED_GREEN 1U
#define LED_BLUE 2U

#define CUATRO_LED_PWM_PERCENT 20U

void led_set(uint8_t color, bool enabled) {
  if (color < 3U) {
#ifdef HW_TYPE_CUATRO
    if (hw_type == HW_TYPE_CUATRO) {
      uint8_t channel = 0U;
      switch (color) {
        case LED_RED:
          channel = 1U;
          break;
        case LED_GREEN:
          channel = 2U;
          break;
        case LED_BLUE:
          channel = 4U;
          break;
        default:
          break;
      }
      pwm_set(TIM3, channel, enabled ? CUATRO_LED_PWM_PERCENT : 0U);
    } else
#endif
    {
      set_gpio_output(current_board->led_GPIO[color], current_board->led_pin[color], !enabled);
    }
  }
}

void led_init(void) {
#ifdef HW_TYPE_CUATRO
  if (hw_type == HW_TYPE_CUATRO) {
    for (uint8_t i = 0U; i<3U; i++) {
      set_gpio_pullup(current_board->led_GPIO[i], current_board->led_pin[i], PULL_NONE);
      set_gpio_output_type(current_board->led_GPIO[i], current_board->led_pin[i], OUTPUT_TYPE_OPEN_DRAIN);
      led_set(i, false);
    }
  } else
#endif
  {
    for (uint8_t i = 0U; i<3U; i++) {
      set_gpio_pullup(current_board->led_GPIO[i], current_board->led_pin[i], PULL_NONE);
      set_gpio_mode(current_board->led_GPIO[i], current_board->led_pin[i], MODE_OUTPUT);
      set_gpio_output_type(current_board->led_GPIO[i], current_board->led_pin[i], OUTPUT_TYPE_OPEN_DRAIN);
      led_set(i, false);
    }
  }
}
