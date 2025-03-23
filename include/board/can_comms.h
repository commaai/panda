#pragma once
#include <stdint.h>

int comms_can_read(uint8_t *data, uint32_t max_len);
void comms_can_write(const uint8_t *data, uint32_t len);
void comms_can_reset(void);
void refresh_can_tx_slots_available(void);