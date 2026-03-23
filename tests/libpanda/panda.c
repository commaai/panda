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
void isotp_tx_comms_resume_usb(void) { };
void isotp_tx_comms_resume_spi(void) { };

#include "health.h"
#include "sys/faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "main_definitions.h"
#include "drivers/can_common.h"

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

#include "comms_definitions.h"
#include "can_comms.h"
#include "isotp.h"

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  int resp_len = 0;
  UNUSED(resp);

  switch (req->request) {
    case 0xc0:
      comms_can_reset();
      comms_isotp_reset();
      break;
    case 0xea:
      if ((req->length == 0U) &&
          (req->param2 == 0U) &&
          ((req->param1 & 0xFF00U) == 0U) &&
          (req->param1 < PANDA_CAN_CNT)) {
        isotp_set_bus((uint8_t)req->param1);
      }
      break;
    case 0xeb:
      if (req->length == 0U) {
        (void)isotp_set_tx_arb_id(((uint32_t)req->param2 << 16U) | req->param1);
      }
      break;
    case 0xec:
      if (req->length == 0U) {
        (void)isotp_set_rx_arb_id(((uint32_t)req->param2 << 16U) | req->param1);
      }
      break;
    case 0xed:
      if ((req->length == 0U) &&
          ((req->param1 & 0xFE00U) == 0U) &&
          ((req->param2 & 0xFE00U) == 0U)) {
        isotp_set_ext_addr(req->param1, req->param2);
      }
      break;
    case 0xee:
      if ((req->length == 0U) &&
          (req->param1 != 0U) &&
          (req->param2 != 0U)) {
        isotp_set_tx_timeouts(req->param1, req->param2);
      }
      break;
    default:
      break;
  }

  return resp_len;
}

void set_microsecond_timer(uint32_t time) {
  timer.CNT = time;
}
