#pragma once

#include <stdint.h>

// Encoder pin map:
// M1 encoder: PB6 -> TIM4_CH1, PB7 -> TIM4_CH2
// M2 encoder: PA6 -> TIM3_CH1, PA7 -> TIM3_CH2

#ifndef MOTOR1_ENCODER_COUNTS_PER_REV
#define MOTOR1_ENCODER_COUNTS_PER_REV 44U
#endif

#ifndef MOTOR2_ENCODER_COUNTS_PER_REV
#define MOTOR2_ENCODER_COUNTS_PER_REV 44U
#endif

#ifndef MOTOR1_GEAR_RATIO
#define MOTOR1_GEAR_RATIO 90U
#endif

#ifndef MOTOR2_GEAR_RATIO
#define MOTOR2_GEAR_RATIO 90U
#endif

#ifndef MOTOR_ENCODER_MIN_DT_US
#define MOTOR_ENCODER_MIN_DT_US 250U
#endif

#ifndef MOTOR_ENCODER_FILTER
#define MOTOR_ENCODER_FILTER 3U
#endif

#ifndef MOTOR_ENCODER_SPEED_ALPHA
#define MOTOR_ENCODER_SPEED_ALPHA 0.2f
#endif

#ifndef MOTOR_ENCODER_ZERO_TIMEOUT_US
#define MOTOR_ENCODER_ZERO_TIMEOUT_US 30000U
#endif

#ifndef MOTOR_ENCODER_SPEED_DECAY
#define MOTOR_ENCODER_SPEED_DECAY 0.5f
#endif

typedef struct {
  TIM_TypeDef *timer;
  GPIO_TypeDef *gpio_a;
  uint8_t pin_a;
  uint8_t af_a;
  GPIO_TypeDef *gpio_b;
  uint8_t pin_b;
  uint8_t af_b;
  uint32_t effective_counts_per_rev;
  uint16_t last_timer_count;
  int32_t accumulated_count;
  int32_t last_speed_count;
  uint32_t last_speed_timestamp_us;
  float cached_speed_rps;
} motor_encoder_state_t;

static motor_encoder_state_t motor_encoders[2] = {
  {
    .timer = TIM4,
    .gpio_a = GPIOB,
    .pin_a = 6U,
    .af_a = GPIO_AF2_TIM4,
    .gpio_b = GPIOB,
    .pin_b = 7U,
    .af_b = GPIO_AF2_TIM4,
    .effective_counts_per_rev = (uint32_t)(((uint64_t)MOTOR1_ENCODER_COUNTS_PER_REV) * ((uint64_t)MOTOR1_GEAR_RATIO)),
  },
  {
    .timer = TIM3,
    .gpio_a = GPIOA,
    .pin_a = 6U,
    .af_a = GPIO_AF2_TIM3,
    .gpio_b = GPIOA,
    .pin_b = 7U,
    .af_b = GPIO_AF2_TIM3,
    .effective_counts_per_rev = (uint32_t)(((uint64_t)MOTOR2_ENCODER_COUNTS_PER_REV) * ((uint64_t)MOTOR2_GEAR_RATIO)),
  },
};

static inline motor_encoder_state_t *get_encoder(uint8_t motor) {
  if ((motor == 0U) || (motor > 2U)) {
    return NULL;
  }
  return &motor_encoders[motor - 1U];
}

static void motor_encoder_configure_timer(motor_encoder_state_t *encoder) {
  encoder->timer->CR1 = 0U;
  encoder->timer->CR2 = 0U;
  encoder->timer->SMCR = 0U;
  encoder->timer->DIER = 0U;
  encoder->timer->SR = 0U;
  encoder->timer->CCMR1 =
      (TIM_CCMR1_CC1S_0) |
      (TIM_CCMR1_CC2S_0) |
      (MOTOR_ENCODER_FILTER << TIM_CCMR1_IC1F_Pos) |
      (MOTOR_ENCODER_FILTER << TIM_CCMR1_IC2F_Pos);
  encoder->timer->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
  encoder->timer->PSC = 0U;
  encoder->timer->ARR = 0xFFFFU;
  encoder->timer->CNT = 0U;
  encoder->last_timer_count = 0U;
  encoder->accumulated_count = 0;
  encoder->last_speed_count = 0;
  encoder->cached_speed_rps = 0.0f;
  encoder->timer->SMCR = (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); // Encoder mode 3 (TI1 + TI2)
  encoder->timer->CR1 = TIM_CR1_CEN;
}

static void motor_encoder_configure_gpio(motor_encoder_state_t *encoder) {
  set_gpio_pullup(encoder->gpio_a, encoder->pin_a, PULL_UP);
  set_gpio_output_type(encoder->gpio_a, encoder->pin_a, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_alternate(encoder->gpio_a, encoder->pin_a, encoder->af_a);

  set_gpio_pullup(encoder->gpio_b, encoder->pin_b, PULL_UP);
  set_gpio_output_type(encoder->gpio_b, encoder->pin_b, OUTPUT_TYPE_PUSH_PULL);
  set_gpio_alternate(encoder->gpio_b, encoder->pin_b, encoder->af_b);
}

static void motor_encoder_init(void) {
  register_set_bits(&(RCC->APB1LENR), RCC_APB1LENR_TIM4EN | RCC_APB1LENR_TIM3EN);
  register_set_bits(&(RCC->APB1LRSTR), RCC_APB1LRSTR_TIM4RST | RCC_APB1LRSTR_TIM3RST);
  register_clear_bits(&(RCC->APB1LRSTR), RCC_APB1LRSTR_TIM4RST | RCC_APB1LRSTR_TIM3RST);

  for (uint8_t i = 0U; i < 2U; i++) {
    motor_encoder_state_t *encoder = &motor_encoders[i];
    motor_encoder_configure_gpio(encoder);
    motor_encoder_configure_timer(encoder);
    encoder->last_speed_timestamp_us = 0U;
  }
}

static inline int32_t motor_encoder_refresh(motor_encoder_state_t *encoder) {
  uint16_t raw = (uint16_t)encoder->timer->CNT;
  int16_t delta = (int16_t)(raw - encoder->last_timer_count);
  if (encoder == &motor_encoders[0]) {
    delta = (int16_t)(-delta);
  }
  encoder->last_timer_count = raw;
  encoder->accumulated_count += (int32_t)delta;
  return encoder->accumulated_count;
}

static int32_t motor_encoder_get_position(uint8_t motor) {
  motor_encoder_state_t *encoder = get_encoder(motor);
  if (encoder == NULL) {
    return 0;
  }
  return motor_encoder_refresh(encoder);
}

static void motor_encoder_reset(uint8_t motor) {
  motor_encoder_state_t *encoder = get_encoder(motor);
  if (encoder == NULL) {
    return;
  }
  encoder->timer->CNT = 0U;
  encoder->last_timer_count = 0U;
  encoder->accumulated_count = 0;
  encoder->last_speed_count = 0;
  encoder->last_speed_timestamp_us = 0U;
  encoder->cached_speed_rps = 0.0f;
}

static float motor_encoder_get_speed_rps(uint8_t motor) {
  motor_encoder_state_t *encoder = get_encoder(motor);
  if (encoder == NULL) {
    return 0.0f;
  }

  motor_encoder_refresh(encoder);

  uint32_t now = microsecond_timer_get();
  if (encoder->last_speed_timestamp_us == 0U) {
    encoder->last_speed_timestamp_us = now;
    encoder->last_speed_count = encoder->accumulated_count;
    encoder->cached_speed_rps = 0.0f;
    return 0.0f;
  }

  uint32_t dt = now - encoder->last_speed_timestamp_us;
  if (dt < MOTOR_ENCODER_MIN_DT_US) {
    return encoder->cached_speed_rps;
  }

  int32_t delta = encoder->accumulated_count - encoder->last_speed_count;
  if (delta == 0) {
    if (dt >= MOTOR_ENCODER_ZERO_TIMEOUT_US) {
      float decay = MOTOR_ENCODER_SPEED_DECAY;
      if (decay < 0.0f) {
        decay = 0.0f;
      } else if (decay > 1.0f) {
        decay = 1.0f;
      }
      encoder->cached_speed_rps *= (1.0f - decay);
      if (encoder->cached_speed_rps < 0.001f) {
        encoder->cached_speed_rps = 0.0f;
      }
      encoder->last_speed_timestamp_us = now;
      encoder->last_speed_count = encoder->accumulated_count;
    }
    return encoder->cached_speed_rps;
  }

  encoder->last_speed_count = encoder->accumulated_count;
  encoder->last_speed_timestamp_us = now;

  float counts_per_second = ((float)delta * 1000000.0f) / (float)dt;
  float new_speed_rps = 0.0f;
  if (encoder->effective_counts_per_rev != 0U) {
    new_speed_rps = counts_per_second / (float)encoder->effective_counts_per_rev;
  }

  float alpha = MOTOR_ENCODER_SPEED_ALPHA;
  if (alpha < 0.0f) {
    alpha = 0.0f;
  } else if (alpha > 1.0f) {
    alpha = 1.0f;
  }

  encoder->cached_speed_rps += alpha * (new_speed_rps - encoder->cached_speed_rps);
  return encoder->cached_speed_rps;
}

static float motor_encoder_get_speed_rpm(uint8_t motor) {
  return motor_encoder_get_speed_rps(motor) * 60.0f;
}
