// The real STM32H7 firmware init, run on the host. The whole peripheral bus is
// backed by plain memory, so the unmodified headers and drivers work as-is and
// tests read the registers straight back.
#include <sys/mman.h>

#include "stm32h7xx.h"

uint8_t bus[0x20000000] __attribute__((aligned(4096)));
#undef PERIPH_BASE
#define PERIPH_BASE ((uintptr_t)bus)

// ARM-only
#define __enable_irq() ((void)0)
#define __disable_irq() ((void)0)
#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#define NVIC_EnableIRQ(irq) ((void)(irq))
#define NVIC_DisableIRQ(irq) ((void)(irq))

#include "config.h"
#include "drivers/led.h"
#include "drivers/pwm.h"

// nobody to talk to
void usb_irqhandler(void) {}
int comms_control_handler(ControlPacket_t *req, uint8_t *resp) { return 0; }
void comms_endpoint2_write(const uint8_t *data, uint32_t len) {}
void comms_can_write(const uint8_t *data, uint32_t len) {}
int comms_can_read(uint8_t *data, uint32_t max_len) { return 0; }

// fresh, zeroed peripherals
void power_cycle(void) {
  mmap(bus, sizeof(bus), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
}

// what the firmware does to the pins before the main loop
void boot(void) {
  early_gpio_float();
  detect_board_type();

  peripherals_init();
  led_init();
  PWR->CR3 |= PWR_CR3_USB33RDY;  // tres waits for the USB LDO
  current_board->init();
  current_board->set_can_mode(CAN_MODE_NORMAL);
  for (uint8_t i = 1U; i <= 4U; i++) {
    current_board->enable_can_transceiver(i, true);
  }
  if (current_board->has_spi) {
    gpio_spi_init();
  }
}
