// Main communications implementation file
#include "main_comms.h"
#include "../board/health.h"
#include "../utils.h"
#include "../drivers/timers.h"
#include "../drivers/uart.h"

static int get_health_pkt(void *dat) {
  COMPILE_TIME_ASSERT(sizeof(struct health_t) <= USBPACKET_MAX_SIZE);
  struct health_t * health = (struct health_t*)dat;

  health->uptime_pkt = uptime_cnt;
  health->voltage_pkt = current_board->read_voltage_mV();
  health->current_pkt = current_board->read_current_mA();

  health->ignition_line_pkt = (uint8_t)(harness_check_ignition());
  health->ignition_can_pkt = ignition_can;

  health->controls_allowed_pkt = controls_allowed;
  health->safety_tx_blocked_pkt = safety_tx_blocked;
  health->safety_rx_invalid_pkt = safety_rx_invalid;
  health->tx_buffer_overflow_pkt = tx_buffer_overflow;
  health->rx_buffer_overflow_pkt = rx_buffer_overflow;
  health->car_harness_status_pkt = harness.status;
  health->safety_mode_pkt = (uint8_t)(current_safety_mode);
  health->safety_param_pkt = current_safety_param;
  health->alternative_experience_pkt = alternative_experience;
  health->power_save_enabled_pkt = power_save_enabled;
  health->heartbeat_lost_pkt = heartbeat_lost;
  health->safety_rx_checks_invalid_pkt = safety_rx_checks_invalid;

  health->spi_error_count_pkt = spi_error_count;

  health->fault_status_pkt = fault_status;
  health->faults_pkt = faults;

  health->interrupt_load_pkt = interrupt_load;

  health->fan_power = fan_state.power;

  health->sbu1_voltage_mV = harness.sbu1_voltage_mV;
  health->sbu2_voltage_mV = harness.sbu2_voltage_mV;

  health->som_reset_triggered = bootkick_reset_triggered;

  health->sound_output_level_pkt = sound_output_level;

  return sizeof(*health);
}

void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  uart_ring *ur = get_ring_by_number(data[0]);
  if ((len != 0U) && (ur != NULL)) {
    if ((data[0] < 2U) || (data[0] >= 4U)) {
      for (uint32_t i = 1; i < len; i++) {
        while (!put_char(ur, data[i])) {
          // wait
        }
      }
    }
  }
}

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  unsigned int resp_len = 0;
  uart_ring *ur = NULL;
  uint32_t time;

#ifdef DEBUG_COMMS
  print("raw control request: "); hexdump(req, sizeof(ControlPacket_t)); print("\n");
  print("- request "); puth(req->request); print("\n");
  print("- param1 "); puth(req->param1); print("\n");
  print("- param2 "); puth(req->param2); print("\n");
#endif

  switch (req->request) {
    case 0xa8:
      time = microsecond_timer_get();
      resp[0] = (time & 0x000000FFU);
      resp[1] = ((time & 0x0000FF00U) >> 8U);
      resp[2] = ((time & 0x00FF0000U) >> 16U);
      resp[3] = ((time & 0xFF000000U) >> 24U);
      resp_len = 4U;
      break;
    case 0xb0:
      current_board->set_ir_power(req->param1);
      break;
    case 0xb1:
      fan_set_power(req->param1);
      break;
    case 0xb2:
      resp[0] = (fan_state.rpm & 0x00FFU);
      resp[1] = ((fan_state.rpm & 0xFF00U) >> 8U);
      resp_len = 2;
      break;
    case 0xb5:
      #ifdef ALLOW_DEBUG
      set_safety_mode(SAFETY_SILENT, 0U);
      set_power_save_state(true);
      stop_mode_requested = true;
      #endif
      break;
    case 0xc0:
      comms_can_reset();
      break;
    case 0xc1:
      resp[0] = hw_type;
      resp_len = 1U;
      break;
    case 0xc2:
      COMPILE_TIME_ASSERT(sizeof(can_health_t) <= USBPACKET_MAX_SIZE);
      if (req->param1 < 3U) {
        update_can_health_pkt(req->param1, 0U);
        can_health[req->param1].can_speed = (bus_config[req->param1].can_speed / 10U);
        can_health[req->param1].can_data_speed = (bus_config[req->param1].can_data_speed / 10U);
        can_health[req->param1].canfd_enabled = bus_config[req->param1].canfd_enabled;
        can_health[req->param1].brs_enabled = bus_config[req->param1].brs_enabled;
        can_health[req->param1].canfd_non_iso = bus_config[req->param1].canfd_non_iso;
        resp_len = sizeof(can_health[req->param1]);
        (void)memcpy(resp, (uint8_t*)(&can_health[req->param1]), resp_len);
      }
      break;
    case 0xc3:
      (void)memcpy(resp, ((uint8_t *)UID_BASE), 12);
      resp_len = 12U;
      break;
    case 0xc4:
      if (req->param1 < NUM_INTERRUPTS) {
        uint32_t load = interrupts[req->param1].call_rate;
        resp[0] = (load & 0x000000FFU);
        resp[1] = ((load & 0x0000FF00U) >> 8U);
        resp[2] = ((load & 0x00FF0000U) >> 16U);
        resp[3] = ((load & 0xFF000000U) >> 24U);
        resp_len = 4U;
      }
      break;
    case 0xc5:
      set_intercept_relay((req->param1 & 0x1U), (req->param1 & 0x2U));
      break;
    case 0xcb:
      resp[0] = (hw_type == HW_TYPE_CLOUDPANDA) ? 1 : 0;
      resp_len = 1U;
      break;
    case 0xcd:
      handle_serial_msg((unsigned char*)&(req->param1), sizeof(req->param1));
      break;
    default:
      break;
  }

  return resp_len;
}
