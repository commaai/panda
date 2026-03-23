#pragma once

// Body and jungle reuse the shared comms plumbing but do not instantiate
// the full ISO-TP state machine yet.

int comms_isotp_read(uint8_t *data, uint32_t max_len) {
  UNUSED(data);
  UNUSED(max_len);
  return 0;
}

void comms_isotp_write(const uint8_t *data, uint32_t len) {
  UNUSED(data);
  UNUSED(len);
}

void comms_isotp_reset(void) {}

bool comms_isotp_can_write_usb(void) {
  return true;
}

bool comms_isotp_can_write_spi(uint32_t len) {
  UNUSED(len);
  return true;
}

void isotp_rx_hook(const CANPacket_t *msg, uint32_t now_us) {
  UNUSED(msg);
  UNUSED(now_us);
}
