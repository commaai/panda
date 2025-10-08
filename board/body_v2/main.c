#include <stdint.h>
#include <stdbool.h>

#include "board/config.h"
#include "board/config.h"

#include "board/boards/board_declarations.h"
#include "board/boards/body_v2.h"
#include "board/main_declarations.h"
#include "board/drivers/interrupts_declarations.h"
#include "board/drivers/led.h"
#include "board/comms_definitions.h"
#include "board/faults_declarations.h"
#include "board/early_init.h"
#include "stm32h7xx.h"
#include "stm32h725xx.h"


typedef struct uart_ring uart_ring;

void usb_irqhandler(void) {
  // Handle USB interrupts
}

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  (void)req;
  (void)resp;
  return 0;
}

void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  (void)data;
  (void)len;
}

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



int main(void) {
  disable_interrupts();

  clock_init();
  peripherals_init();

  current_board = &board_body_v2;
  hw_type = HW_TYPE_BODY_V2;

  led_init();
  
  while (1) {
    led_set(LED_RED, true);
    delay(12000000U);  // 1/10th of previous delay
    led_set(LED_RED, false);
    delay(12000000U);
  }

  return 0;
}