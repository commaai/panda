#pragma once

#define LED_RED 0U
#define LED_GREEN 1U
#define LED_BLUE 2U

#define LED_PWM_POWER 2U

void led_set(uint8_t color, bool enabled);
void led_init(void);
