#include "fake_stm.h"
#include "config.h"
#include "can.h"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
//int safety_tx_hook(CANPacket_t *to_send) { return 1; }

typedef struct harness_configuration harness_configuration;
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "sys/faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "main_definitions.h"
#include "drivers/registers.h"
#include "drivers/gpio.h"
#include "drivers/can_common.h"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

GPIO_TypeDef *fake_stm_get_gpio(uint8_t port) {
  switch (port) {
    case 0U:
      return GPIOA;
    case 1U:
      return GPIOB;
    case 2U:
      return GPIOC;
    case 3U:
      return GPIOD;
    case 4U:
      return GPIOE;
    case 5U:
      return GPIOF;
    case 6U:
      return GPIOG;
    case 7U:
      return GPIOH;
    default:
      return NULL;
  }
}

void fake_stm_set_gpio_output(uint8_t port, uint8_t pin, bool enabled) {
  set_gpio_output(fake_stm_get_gpio(port), pin, enabled);
}

#include "comms_definitions.h"
#include "can_comms.h"
