#pragma once

#include "board/debug/can_replay_data.h"

#define CAN_REPLAY_INTERVAL_US 10000U

bool can_replay_enabled = false;
uint32_t can_replay_event_idx = 0U;
uint32_t can_replay_msg_idx = 0U;
uint32_t can_replay_next_frame_ts = 0U;

static void can_replay_reset(void) {
  can_replay_event_idx = 0U;
  can_replay_msg_idx = 0U;
  can_replay_next_frame_ts = microsecond_timer_get();
}

void can_replay_set_enabled(bool enabled) {
  can_replay_enabled = enabled;
  can_replay_reset();

  if (enabled) {
    can_clear(&can_rx_q);
    set_power_save_state(false);
  }
}

uint32_t can_replay_status(uint8_t *resp) {
  resp[0] = can_replay_enabled ? 1U : 0U;
  WORD_TO_BYTE_ARRAY(&resp[1], can_replay_event_idx);
  WORD_TO_BYTE_ARRAY(&resp[5], CAN_REPLAY_EVENT_COUNT);
  WORD_TO_BYTE_ARRAY(&resp[9], CAN_REPLAY_MSG_COUNT);
  return 13U;
}

static void can_replay_push_msg(uint32_t record_idx) {
  uint32_t offset = CAN_REPLAY_EVENT_COUNT + (record_idx * CAN_REPLAY_RECORD_SIZE);
  uint16_t address = (uint16_t)(can_replay_data[offset] | (can_replay_data[offset + 1U] << 8U));
  uint8_t bus_len = can_replay_data[offset + 2U];
  uint8_t len = bus_len & 0xFU;
  uint8_t bus = (bus_len >> 4U) & 0x7U;

  CANPacket_t to_push = {0};
  to_push.fd = 0U;
  to_push.returned = 0U;
  to_push.rejected = 0U;
  to_push.extended = 0U;
  to_push.addr = address;
  to_push.bus = bus;
  to_push.data_len_code = len;
  (void)memcpy(to_push.data, &can_replay_data[offset + 3U], len);
  can_set_checksum(&to_push);

  if (bus < PANDA_CAN_CNT) {
    uint8_t can_number = CAN_NUM_FROM_BUS_NUM(bus);
    can_health[can_number].total_rx_cnt += 1U;
  }

  safety_rx_invalid += safety_rx_hook(&to_push) ? 0U : 1U;
  ignition_can_hook(&to_push);
  led_set(LED_BLUE, true);
  rx_buffer_overflow += can_push(&can_rx_q, &to_push) ? 0U : 1U;
}

static void can_replay_push_frame(void) {
  uint8_t msg_count = can_replay_data[can_replay_event_idx];

  for (uint8_t i = 0U; i < msg_count; i++) {
    can_replay_push_msg(can_replay_msg_idx);
    can_replay_msg_idx += 1U;
  }

  can_replay_event_idx += 1U;
  if (can_replay_event_idx >= CAN_REPLAY_EVENT_COUNT) {
    can_replay_event_idx = 0U;
    can_replay_msg_idx = 0U;
  }
}

void can_replay_tick(void) {
  COMPILE_TIME_ASSERT(CAN_REPLAY_RECORD_SIZE == 11U);
  COMPILE_TIME_ASSERT(sizeof(can_replay_data) == CAN_REPLAY_DATA_SIZE);

  if (can_replay_enabled) {
    uint32_t now = microsecond_timer_get();
    while ((int32_t)(now - can_replay_next_frame_ts) >= 0) {
      can_replay_push_frame();
      can_replay_next_frame_ts += CAN_REPLAY_INTERVAL_US;
      now = microsecond_timer_get();
    }
  }
}
