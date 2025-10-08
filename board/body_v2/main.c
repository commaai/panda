

#include <stdint.h>

#include "board/config.h"


void led_init(void);
void led_set(uint8_t color, bool enabled);
#include "board/early_init.h"
#include "stm32h7xx.h"
#include "stm32h725xx.h"

typedef struct uart_ring uart_ring;
void usb_irqhandler(void) {
  // no-op stub for minimal bring-up
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

void led_init(void) {}

void led_set(uint8_t color, bool enabled) {
  (void)color;
  (void)enabled;
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

#define LED_GPIO         GPIOC
#define LED_PIN          7U
#define LED_PORT_ENABLE  RCC_AHB4ENR_GPIOCEN

static inline void led_gpio_init(void) {
  RCC->AHB4ENR |= LED_PORT_ENABLE;
  __DSB();
  __ISB();

  const uint32_t pos = LED_PIN * 2U;

  LED_GPIO->MODER = (LED_GPIO->MODER & ~(0x3U << pos)) | (0x1U << pos);
  LED_GPIO->OTYPER &= ~(1U << LED_PIN);
  LED_GPIO->OSPEEDR = (LED_GPIO->OSPEEDR & ~(0x3U << pos)) | (0x1U << pos);
  LED_GPIO->PUPDR &= ~(0x3U << pos);
}

static inline void led_on(void) {
  LED_GPIO->BSRR = (1U << LED_PIN);
}

static inline void led_off(void) {
  LED_GPIO->BSRR = (1U << (LED_PIN + 16U));
}

void __initialize_hardware_early(void) {
  early_initialization();
}

int main(void) {
  init_interrupts(true);
  disable_interrupts();

  clock_init();
  peripherals_init();

  led_gpio_init();

  enable_interrupts();

  while (1) {
    led_on();
    delay(250000U);
    led_off();
    delay(250000U);
  }

  return 0;
}