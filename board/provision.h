// this is where we manage the dongle ID assigned during our
// manufacturing. aside from this, there's a UID for the MCU

#include <stdint.h>

#define PROVISION_CHUNK_LEN 0x20

void get_provision_chunk(uint8_t *resp);