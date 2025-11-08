#include <stdint.h>
#include <stdbool.h>

#include "board/config.h"
#include "board/drivers/led.h"
#include "board/drivers/pwm.h"
#include "board/drivers/usb.h"
#include "board/early_init.h"
#include "board/obj/gitversion.h"
#include "board/body/motor_control.h"
#include "board/body/can.h"
#include "opendbc/safety/safety.h"
#include "board/drivers/can_common.h"
#include "board/drivers/fdcan.h"
#include "board/can_comms.h"
#include "board/body/dotstar.h"

extern int _app_start[0xc000];

#include "board/body/main_comms.h"

static volatile uint32_t tick_count = 0U;
static volatile uint32_t pd8_press_timestamp_us = 0U;
static volatile bool ignition = false;
static volatile bool plug_charging = false;

void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (get_char(ring, &rcv)) {
    (void)injectc(ring, rcv);
  }
}

void __attribute__ ((noinline)) enable_fpu(void) {
  SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
}

void __initialize_hardware_early(void) {
  enable_fpu();
  early_initialization();
}

void tick_handler(void) {
  if (TICK_TIMER->SR != 0) {
    if (can_health[0].transmit_error_cnt >= 128) {
      (void)llcan_init(CANIF_FROM_CAN_NUM(0));
    }
    static bool led_on = false;
    led_set(LED_RED, led_on);
    led_on = !led_on;
    tick_count++;
  }
  TICK_TIMER->SR = 0;
}

static void exti9_5_handler(void) {
  if ((EXTI->PR1 & (1U << 8)) != 0U) {
    EXTI->PR1 = (1U << 8);

    static uint32_t last_press_event_us = 0U;
    static uint32_t last_release_event_us = 0U;
    const uint32_t debounce_us = 200000U; // 200 ms

    uint32_t now = microsecond_timer_get();
    bool pressed = (get_gpio_input(GPIOD, 8U) == 0);

    if (pressed) {
      if ((last_press_event_us != 0U) && (get_ts_elapsed(now, last_press_event_us) < debounce_us)) {
        return;
      }
      last_press_event_us = now;
      pd8_press_timestamp_us = now;
      ignition = !ignition;
    } else {
      if ((last_release_event_us != 0U) && (get_ts_elapsed(now, last_release_event_us) < debounce_us)) {
        return;
      }
      last_release_event_us = now;
      pd8_press_timestamp_us = 0U;
    }
  }
}

static void exti15_10_handler(void) {
  if ((EXTI->PR1 & (1U << 13)) != 0U) {
    EXTI->PR1 = (1U << 13);
    plug_charging = (get_gpio_input(GPIOE, 13U) != 0);
  }
}

int main(void) {
  disable_interrupts();
  init_interrupts(true);

  clock_init();
  peripherals_init();

  current_board = &board_body;
  hw_type = HW_TYPE_BODY;

  current_board->init();

  REGISTER_INTERRUPT(EXTI9_5_IRQn, exti9_5_handler, 10000U, FAULT_INTERRUPT_RATE_EXTI);
  NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
  NVIC_EnableIRQ(EXTI9_5_IRQn);

  REGISTER_INTERRUPT(EXTI15_10_IRQn, exti15_10_handler, 10000U, FAULT_INTERRUPT_RATE_EXTI);
  NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
  NVIC_EnableIRQ(EXTI15_10_IRQn);

  REGISTER_INTERRUPT(TICK_TIMER_IRQ, tick_handler, 10U, FAULT_INTERRUPT_RATE_TICK);

  led_init();
  microsecond_timer_init();
  tick_timer_init();
  usb_init();
  body_can_init();
  dotstar_init();

  plug_charging = (get_gpio_input(GPIOE, 13U) != 0);

  enable_interrupts();

  while (true) {
    uint32_t now = microsecond_timer_get();
    if (plug_charging) {
      dotstar_apply_breathe((dotstar_rgb_t){255U, 40U, 0U}, now, 2000000U);
      motor_set_speed(BODY_MOTOR_LEFT, 0);
      motor_set_speed(BODY_MOTOR_RIGHT, 0);
    } else if (ignition) {
      motor_speed_controller_update(now);
      dotstar_run_rainbow(now);
    } else if (!ignition) {
      dotstar_apply_breathe((dotstar_rgb_t){0U, 255U, 10U}, now, 1500000U);
      motor_set_speed(BODY_MOTOR_LEFT, 0);
      motor_set_speed(BODY_MOTOR_RIGHT, 0);
    }

    if (ignition) {
      body_can_periodic(now);
    }

    dotstar_show();

    if ((pd8_press_timestamp_us != 0U) && (now >= pd8_press_timestamp_us) && (get_ts_elapsed(now, pd8_press_timestamp_us) >= 3000000U)) {
      set_gpio_output(GPIOD, 9U, false);
    }
  }

  return 0;
}
