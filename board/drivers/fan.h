#include "board/drivers/drivers.h"

struct fan_state_t fan_state;

static const uint8_t FAN_TICK_FREQ = 8U;

void fan_set_power(uint8_t percentage) {
  if (percentage > 0U) {
    fan_state.power = CLAMP(percentage, 20U, 100U);
  } else {
    fan_state.power = 0U;
  }
}

// TACH interrupt handler
static void EXTI2_IRQ_Handler(void) {
  volatile unsigned int pr = EXTI->PR1 & (1U << 2);
  if ((pr & (1U << 2)) != 0U) {
    fan_state.tach_counter++;
  }
  EXTI->PR1 = (1U << 2);
}

void fan_init(void) {
  fan_state.cooldown_counter = current_board->fan_enable_cooldown_time * FAN_TICK_FREQ;
  // 12000RPM * 4 tach edges / 60 seconds
  REGISTER_INTERRUPT(EXTI2_IRQn, EXTI2_IRQ_Handler, 1000U, FAULT_INTERRUPT_RATE_TACH)

  // Init PWM speed control
  pwm_init(TIM3, 3);

  // Init TACH interrupt
  register_set(&(SYSCFG->EXTICR[0]), SYSCFG_EXTICR1_EXTI2_PD, 0xF00U);
  register_set_bits(&(EXTI->IMR1), (1U << 2));
  register_set_bits(&(EXTI->RTSR1), (1U << 2));
  register_set_bits(&(EXTI->FTSR1), (1U << 2));
  NVIC_EnableIRQ(EXTI2_IRQn);
}

// Call this at FAN_TICK_FREQ
void fan_tick(void) {
  if (current_board->has_fan) {
    // Measure fan RPM
    uint16_t fan_rpm_fast = fan_state.tach_counter * (60U * FAN_TICK_FREQ / 4U);   // 4 interrupts per rotation
    fan_state.tach_counter = 0U;
    fan_state.rpm = (fan_rpm_fast + (3U * fan_state.rpm)) / 4U;

    #ifdef DEBUG_FAN
      puth(fan_rpm_fast);
      print(" "); puth(fan_state.power);
      print("\n");
    #endif

    // Cooldown counter to prevent noise on tachometer line.
    if (fan_state.power > 0U) {
      fan_state.cooldown_counter = current_board->fan_enable_cooldown_time * FAN_TICK_FREQ;
    } else {
      if (fan_state.cooldown_counter > 0U) {
        fan_state.cooldown_counter--;
      }
    }

    // Set PWM and enable line
    pwm_set(TIM3, 3, fan_state.power);
    current_board->set_fan_enabled((fan_state.power > 0U) || (fan_state.cooldown_counter > 0U));
  }
}
