#ifndef PANDA_CRC_H
#define PANDA_CRC_H

#include <stdint.h>

uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly);

#endif
