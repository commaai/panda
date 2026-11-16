#ifndef BOARD_PROVISION_H
#define BOARD_PROVISION_H

// this is where we manage the dongle ID assigned during our
// manufacturing. aside from this, there's a UID for the MCU

#define PROVISION_CHUNK_LEN 0x20

static void get_provision_chunk(uint8_t *resp) {
  const unsigned char unprovisioned_text[] = "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";

  (void)memcpy(resp, (uint8_t *)PROVISION_CHUNK_ADDRESS, PROVISION_CHUNK_LEN);
  if (bytes_equal(resp, unprovisioned_text, PROVISION_CHUNK_LEN)) {
    (void)memcpy(resp, "unprovisioned\x00\x00\x00" "testing123\x00\x00\xa3\xa6\x99\xec", PROVISION_CHUNK_LEN);
  }
}

#endif  // BOARD_PROVISION_H
