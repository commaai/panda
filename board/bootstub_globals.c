#include "bootstub_declarations.h"
#include "board/drivers/fan_declarations.h"
#include "board/drivers/harness_declarations.h"
#include "board/boards/board_declarations.h"
#include <stddef.h>

// Global variable definitions for BOOTSTUB
struct fan_state_t fan_state;
struct harness_t harness;
uart_ring uart_ring_som_debug;
uint8_t hw_type = 0;
board *current_board = NULL;

// Minimal board definitions for BOOTSTUB
static harness_configuration minimal_harness_config = {0};
board board_dos = {
  .harness_config = &minimal_harness_config,
  .has_spi = false,
  .fan_max_rpm = 0,
  .fan_max_pwm = 0,
  .avdd_mV = 3300U,
  .fan_stall_recovery = false,
  .fan_enable_cooldown_time = 0,
  .init = NULL,
  .init_bootloader = NULL,
  .enable_can_transceiver = NULL,
  .set_can_mode = NULL,
  .read_voltage_mV = NULL,
  .read_current_mA = NULL,
  .set_fan_enabled = NULL,
  .set_ir_power = NULL,
  .set_siren = NULL,
  .set_bootkick = NULL,
  .read_som_gpio = NULL,
  .set_amp_enabled = NULL
};

#ifdef STM32F4
#include "board/stm32f4/inc/stm32f4xx.h"
USB_OTG_GlobalTypeDef *USBx = USB_OTG_FS;
void usb_init(void) {
  // Minimal USB init for BOOTSTUB
}
#elif defined(STM32H7)
#include "board/stm32h7/inc/stm32h7xx.h"
USB_OTG_GlobalTypeDef *USBx = USB_OTG_HS;
void usb_init(void) {
  // Minimal USB init for BOOTSTUB
}
#endif

// Function implementations for BOOTSTUB
void print(const char *a){ (void)(a); }
void puth(uint8_t i){ (void)(i); }
void puth2(uint8_t i){ (void)(i); }
void puth4(uint8_t i){ (void)(i); }
void hexdump(const void *a, int l){ (void)(a); (void)(l); }
void uart_init(uart_ring *q, int baud) { (void)(q); (void)(baud); }

// Memory functions for BOOTSTUB
void *memset(void *str, int c, unsigned int n) {
  uint8_t *s = str;
  for (unsigned int i = 0; i < n; i++) {
    *s = c;
    s++;
  }
  return str;
}

void *memcpy(void *dest, const void *src, unsigned int len) {
  unsigned int n = len;
  uint8_t *d8 = dest;
  const uint8_t *s8 = src;
  while (n-- > 0U) {
    *d8 = *s8; d8++; s8++;
  }
  return dest;
}

int memcmp(const void * ptr1, const void * ptr2, unsigned int num) {
  int ret = 0;
  const uint8_t *p1 = ptr1;
  const uint8_t *p2 = ptr2;
  for (unsigned int i = 0; (i < num) && (ret == 0); i++) {
    ret = p1[i] - p2[i];
  }
  return ret;
}