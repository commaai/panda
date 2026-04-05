#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

#include <stdint.h>
#include "board/body/motor_common.h"

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

typedef struct {
  TIM_TypeDef *timer;
  GPIO_TypeDef *pin_a_port;
  uint8_t pin_a;
  uint8_t pin_a_af;
  GPIO_TypeDef *pin_b_port;
  uint8_t pin_b;
  uint8_t pin_b_af;
  int8_t direction;
  uint32_t counts_per_output_rev;
  uint32_t min_dt_us;
  float speed_alpha;
  uint32_t filter;
} motor_encoder_config_t;

typedef struct {
  const motor_encoder_config_t *config;
  uint16_t last_timer_count;
  int32_t accumulated_count;
  int32_t last_speed_count;
  uint32_t last_speed_timestamp_us;
  float cached_speed_rps;
} motor_encoder_state_t;

void motor_encoder_init(void);
int32_t motor_encoder_refresh(motor_encoder_state_t *state);
int32_t motor_encoder_get_position(uint8_t motor);
void motor_encoder_reset(uint8_t motor);
float motor_encoder_get_speed_rpm(uint8_t motor);

#endif
