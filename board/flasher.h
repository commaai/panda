#pragma once

void spi_init(void);

int comms_control_handler(ControlPacket_t *req, uint8_t *resp);

void comms_can_write(const uint8_t *data, uint32_t len);

int comms_can_read(uint8_t *data, uint32_t max_len);

void refresh_can_tx_slots_available(void);

void comms_endpoint2_write(const uint8_t *data, uint32_t len);

void soft_flasher_start(void);
