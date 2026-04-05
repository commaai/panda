// cppcheck-suppress-file misra-c2012-2.3
#include "board/config.h"
#include "board/utils.h"
#include "board/libc.h"
#include "board/main_declarations.h"
#include "board/main_comms.h"
#include "board/drivers/interrupts.h"
#include "board/drivers/usb.h"
#include "board/drivers/spi.h"
#include "board/drivers/fan.h"
#include "board/drivers/harness.h"
#include "board/sys/power_saving.h"
#include "board/provision.h"
#include "board/can_comms.h"
#include "board/early_init.h"
#include "board/flasher.h"
#include "board/obj/gitversion.h"
#include "board/safety_mode_wrapper.h"
#include "opendbc/safety/declarations.h"

void get_health_pkt(struct health_t *health) {
  (void)memset(health, 0, sizeof(struct health_t));
  health->uptime_pkt = uptime_cnt;
  health->voltage_pkt = current_board->read_voltage_mV();
  health->current_pkt = current_board->read_current_mA();
  health->safety_tx_blocked_pkt = (uint32_t)safety_tx_blocked;
  health->safety_rx_invalid_pkt = (uint32_t)safety_rx_invalid;
  health->tx_buffer_overflow_pkt = 0; // TODO
  health->rx_buffer_overflow_pkt = 0; // TODO
  health->faults_pkt = faults;
  health->ignition_line_pkt = 0; // TODO: handle hardware ignition line if needed
  health->ignition_can_pkt = ignition_can ? 1U : 0U;
  health->controls_allowed_pkt = controls_allowed ? 1U : 0U;
  health->car_harness_status_pkt = (uint8_t)harness_check_ignition();
  health->safety_mode_pkt = (uint8_t)current_safety_mode;
  health->safety_param_pkt = (uint16_t)current_safety_param;
  health->fault_status_pkt = fault_status;
  health->power_save_enabled_pkt = power_save_enabled ? 1U : 0U;
  health->heartbeat_lost_pkt = heartbeat_lost ? 1U : 0U;
  health->alternative_experience_pkt = (uint16_t)alternative_experience;
  health->interrupt_load_pkt = interrupt_load;
  health->fan_power = fan_state.power;
  health->safety_rx_checks_invalid_pkt = 0; // TODO
  health->spi_error_count_pkt = spi_error_count;
}

void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  UNUSED(data);
  UNUSED(len);
}

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  int resp_len = 0;
  uint16_t fan_speed_req;

  switch (req->request) {
    // **** 0xc1: get hardware type
    case 0xc1:
      resp[0] = hw_type;
      resp_len = 1;
      break;
    // **** 0xd0: fetch serial number
    case 0xd0:
      if (req->param1 == 1U) {
        (void)memcpy(resp, (void *)DEVICE_SERIAL_NUMBER_ADDRESS, 0x10);
        resp_len = 0x10;
      } else {
        get_provision_chunk(resp);
        resp_len = PROVISION_CHUNK_LEN;
      }
      break;
    // **** 0xd1: enter bootloader mode
    case 0xd1:
      switch (req->param1) {
        case 0:
          enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
          NVIC_SystemReset();
          break;
        case 1:
          enter_bootloader_mode = ENTER_SOFTLOADER_MAGIC;
          NVIC_SystemReset();
          break;
        default: break;
      }
      break;
    // **** 0xd2: get health packet
    case 0xd2:
      get_health_pkt((struct health_t *)resp);
      resp_len = sizeof(struct health_t);
      break;
    // **** 0xd6: get version
    case 0xd6:
      COMPILE_TIME_ASSERT(sizeof(gitversion) <= USBPACKET_MAX_SIZE);
      (void)memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = (int)sizeof(gitversion) - 1;
      break;
    // **** 0xd8: reset ST
    case 0xd8:
      NVIC_SystemReset();
      break;
    // **** 0xd9: set safety mode
    case 0xd9:
      set_safety_mode((uint16_t)req->param1, (uint32_t)req->param2);
      break;
    // **** 0xda: car specific hook
    case 0xda:
      if (hw_type == HW_TYPE_TRES) {
        tres_set_can_mode((uint8_t)req->param1);
      }
      break;
    // **** 0xdc: set safety param
    case 0xdc:
      set_safety_param((uint32_t)req->param1);
      break;
    // **** 0xdd: get car specific hook
    case 0xdd:
      if (hw_type == HW_TYPE_TRES) {
        resp[0] = tres_read_som_gpio() ? 1U : 0U;
        resp_len = 1;
      }
      break;
    // **** 0xdf: set fan speed
    case 0xdf:
      fan_speed_req = (uint16_t)req->param1;
      fan_set_power((uint8_t)fan_speed_req);
      break;
    // **** 0xe0: set usb power
    case 0xe0:
      // current_board->set_usb_power(req->param1 != 0U);
      break;
    // **** 0xe1: set internal relay
    case 0xe1:
      set_intercept_relay(req->param1 != 0U, req->param2 != 0U);
      break;
    // **** 0xe2: set heartbeat_disabled
    case 0xe2:
      heartbeat_disabled = (req->param1 != 0U);
      break;
    // **** 0xe4: set siren_enabled
    case 0xe4:
      siren_enabled = (req->param1 != 0U);
      break;
    // **** 0xe5: set power save state
    case 0xe5:
      set_power_save_state(req->param1 != 0U);
      break;
    // **** 0xe6: get car specific hook
    case 0xe6:
      if (current_board->harness_config != NULL) {
        resp[0] = (uint8_t)harness_check_ignition();
        resp_len = 1;
      }
      break;
    // **** 0xe7: set car specific hook
    case 0xe7:
      if (current_board->harness_config != NULL) {
        can_set_orientation((uint8_t)req->param1);
      }
      break;
    // **** 0xf1: clear can ring
    case 0xf1:
      can_clear_send(cans[req->param1], (uint8_t)req->param1);
      break;
    // **** 0xf2: get can ring available slots
    case 0xf2:
      resp[0] = can_tx_slots_available(cans[req->param1]);
      resp_len = 1;
      break;
    default:
      resp_len = current_board->board_comms_handler(req, resp);
      break;
  }
  return resp_len;
}
