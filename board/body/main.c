#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "board/config.h"
#include "board/main_declarations.h"
#include "board/drivers/interrupts_declarations.h"
#include "board/drivers/led.h"
#include "board/comms_definitions.h"
#include "board/faults_declarations.h"
#include "board/early_init.h"
#include "board/drivers/usb.h"
#include "board/provision.h"
#include "board/obj/gitversion.h"

extern int _app_start[0xc000];

typedef struct uart_ring uart_ring;

void refresh_can_tx_slots_available(void) {}

void comms_can_write(const uint8_t *data, uint32_t len) {
  (void)data;
  (void)len;
}

int comms_can_read(uint8_t *data, uint32_t max_len) {
  (void)data;
  (void)max_len;
  return 0;
}

void comms_can_reset(void) {}

#include "board/body/main_comms.h"

void debug_ring_callback(uart_ring *ring) {
  (void)ring;
}

void pwm_init(TIM_TypeDef *TIM, uint8_t channel) {
  (void)TIM;
  (void)channel;
}

void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage) {
  (void)TIM;
  (void)channel;
  (void)percentage;
}

void __initialize_hardware_early(void) {
  early_initialization();
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
  hw_type = HW_TYPE_BODY_V2;

  led_init();
  microsecond_timer_init();
  tick_timer_init();

  REGISTER_INTERRUPT(TICK_TIMER_IRQ, tick_handler, 10U, FAULT_INTERRUPT_RATE_TICK);

  for (int i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
  }

  NVIC_EnableIRQ(TICK_TIMER_IRQ);
  NVIC_EnableIRQ(OTG_HS_IRQn);

  usb_init();
  enable_interrupts();

  while (true) {
    // Main loop
  }

  return 0;
}
