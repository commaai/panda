#include <stdint.h>
#include <stdbool.h>
#include "board/config.h"
#include "board/main_declarations.h"
#include "board/drivers/interrupts_declarations.h"
#include "board/drivers/led.h"
#include "board/drivers/pwm.h"
#include "board/drivers/usb.h"
#include "board/early_init.h"
#include "board/provision.h"
#include "board/comms_definitions.h"
#include "board/faults_declarations.h"
#include "board/obj/gitversion.h"
#include "board/body/boards/motor_control.h"
#include "board/body/can.h"
#include "opendbc/safety/safety.h"
#include "board/health.h"
#include "board/drivers/can_common.h"
#include "board/drivers/fdcan.h"
#include "board/can_comms.h"

extern int _app_start[0xc000];

//typedef struct uart_ring uart_ring;

#include "board/body/main_comms.h"
#include "board/body/can.c"

void debug_ring_callback(uart_ring *ring) {
  (void)ring;
}

void __initialize_hardware_early(void) {
  early_initialization();
}

void __attribute__ ((noinline)) enable_fpu(void) {
  // enable the FPU
  SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
}

volatile uint32_t tick_count = 0;

void tick_handler(void) {
  TICK_TIMER->SR = 0;
  tick_count++;
  
  static bool led_on = false;
  led_set(LED_RED, led_on);
  led_on = !led_on;
}

int main(void) {
  init_interrupts(true);
  disable_interrupts();

  clock_init();
  peripherals_init();

  current_board = &board_body;
  hw_type = HW_TYPE_BODY;

  current_board->init();

  // we have an FPU, let's use it!
  enable_fpu();

  led_init();
  microsecond_timer_init();
  REGISTER_INTERRUPT(TICK_TIMER_IRQ, tick_handler, 10U, FAULT_INTERRUPT_RATE_TICK);
  tick_timer_init();

  for (int i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
  }

  NVIC_EnableIRQ(TICK_TIMER_IRQ);
  NVIC_EnableIRQ(OTG_HS_IRQn);

  body_can_init();

  usb_init();

  enable_interrupts();

  //motor_speed_controller_set_target_rpm(1, 50);
  //motor_speed_controller_set_target_rpm(2, 50);

  while (true) {
    uint32_t now = microsecond_timer_get();
    motor_speed_controller_update(now);
    body_can_periodic(now);
  }

  return 0;
}
