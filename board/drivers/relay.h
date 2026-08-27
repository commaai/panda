#pragma once

#define RELAY_CHECK_UNKNOWN 0U
#define RELAY_CHECK_PASS 1U
#define RELAY_CHECK_FAIL 2U

#define RELAY_MATCH_CACHE_SIZE 8U
#define RELAY_MATCH_TIMEOUT_US 1000U
#define RELAY_SETTLE_TIMEOUT_US 1000000U
#define RELAY_OBSERVATION_TIMEOUT_US 3000000U
#define RELAY_MIN_MATCHES 5U
#define RELAY_MIN_MESSAGES 100U

// A closed harness relay electrically joins buses 0 and 2, so both CAN
// controllers receive the same frames. An open relay isolates the buses.
typedef struct {
  CANPacket_t packet;
  uint32_t timestamp;
  bool valid;
} relay_packet_t;

typedef struct {
  bool relay_open;
  bool observation_complete;
  uint32_t state_change_timestamp;
  uint16_t rx_count[2];
  uint8_t match_count;
  uint8_t cache_index[2];
  relay_packet_t cache[2][RELAY_MATCH_CACHE_SIZE];
  uint8_t closed_check;
  uint8_t open_check;
} relay_monitor_t;

extern relay_monitor_t relay_monitor;
relay_monitor_t relay_monitor;

static bool relay_packets_equal(const CANPacket_t *a, const CANPacket_t *b) {
  bool equal = (a->fd == b->fd) &&
               (a->extended == b->extended) &&
               (a->addr == b->addr) &&
               (a->data_len_code == b->data_len_code);

  for (uint8_t i = 0U; i < GET_LEN(a); i++) {
    if (a->data[i] != b->data[i]) {
      equal = false;
    }
  }
  return equal;
}

void relay_monitor_set_state(bool relay_open) {
  relay_monitor.relay_open = relay_open;
  relay_monitor.observation_complete = false;
  relay_monitor.state_change_timestamp = microsecond_timer_get();
  relay_monitor.rx_count[0] = 0U;
  relay_monitor.rx_count[1] = 0U;
  relay_monitor.match_count = 0U;
  relay_monitor.cache_index[0] = 0U;
  relay_monitor.cache_index[1] = 0U;

  for (uint8_t bus = 0U; bus < 2U; bus++) {
    for (uint8_t i = 0U; i < RELAY_MATCH_CACHE_SIZE; i++) {
      relay_monitor.cache[bus][i].valid = false;
    }
  }
}

void relay_monitor_init(void) {
  relay_monitor.closed_check = RELAY_CHECK_UNKNOWN;
  relay_monitor.open_check = RELAY_CHECK_UNKNOWN;
  relay_monitor_set_state(false);
}

void relay_monitor_rx(const CANPacket_t *msg) {
  const uint32_t now = microsecond_timer_get();
  const bool valid_packet = !relay_monitor.observation_complete &&
                            (get_ts_elapsed(now, relay_monitor.state_change_timestamp) >= RELAY_SETTLE_TIMEOUT_US) &&
                            ((msg->bus == 0U) || (msg->bus == 2U));
  if (valid_packet) {
    const uint8_t bus = msg->bus / 2U;
    const uint8_t other_bus = 1U - bus;
    if (relay_monitor.rx_count[bus] < UINT16_MAX) {
      relay_monitor.rx_count[bus] += 1U;
    }

    for (uint8_t i = 0U; i < RELAY_MATCH_CACHE_SIZE; i++) {
      relay_packet_t *cached = &relay_monitor.cache[other_bus][i];
      if (cached->valid &&
          (get_ts_elapsed(now, cached->timestamp) <= RELAY_MATCH_TIMEOUT_US) &&
          relay_packets_equal(msg, &cached->packet)) {
        if (relay_monitor.match_count < UINT8_MAX) {
          relay_monitor.match_count += 1U;
        }
        cached->valid = false;
        break;
      }
    }

    relay_packet_t *cached = &relay_monitor.cache[bus][relay_monitor.cache_index[bus]];
    cached->packet = *msg;
    cached->timestamp = now;
    cached->valid = true;
    relay_monitor.cache_index[bus] = (relay_monitor.cache_index[bus] + 1U) % RELAY_MATCH_CACHE_SIZE;
  }
}

void relay_monitor_tick(void) {
  if (!relay_monitor.observation_complete) {
    const uint32_t elapsed = get_ts_elapsed(microsecond_timer_get(), relay_monitor.state_change_timestamp);
    if (elapsed >= RELAY_SETTLE_TIMEOUT_US) {
      uint8_t result = RELAY_CHECK_UNKNOWN;
      if (relay_monitor.match_count >= RELAY_MIN_MATCHES) {
        result = relay_monitor.relay_open ? RELAY_CHECK_FAIL : RELAY_CHECK_PASS;
      } else if (elapsed >= (RELAY_SETTLE_TIMEOUT_US + RELAY_OBSERVATION_TIMEOUT_US)) {
        const bool enough_traffic = relay_monitor.relay_open ?
                                    ((relay_monitor.rx_count[0] >= RELAY_MIN_MESSAGES) ||
                                     (relay_monitor.rx_count[1] >= RELAY_MIN_MESSAGES)) :
                                    ((relay_monitor.rx_count[0] >= RELAY_MIN_MESSAGES) &&
                                     (relay_monitor.rx_count[1] >= RELAY_MIN_MESSAGES));
        if (enough_traffic) {
          result = relay_monitor.relay_open ? RELAY_CHECK_PASS : RELAY_CHECK_FAIL;
        }
      } else {
      }

      if (result != RELAY_CHECK_UNKNOWN) {
        if (relay_monitor.relay_open) {
          relay_monitor.open_check = result;
        } else {
          relay_monitor.closed_check = result;
        }
        relay_monitor.observation_complete = true;
      } else if (elapsed >= (RELAY_SETTLE_TIMEOUT_US + RELAY_OBSERVATION_TIMEOUT_US)) {
        relay_monitor.observation_complete = true;
      } else {
      }
    }
  }
}

bool relay_monitor_malfunction(void) {
  return (relay_monitor.closed_check == RELAY_CHECK_FAIL) ||
         (relay_monitor.open_check == RELAY_CHECK_FAIL);
}
