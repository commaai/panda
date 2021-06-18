uint8_t crc_checksum(uint8_t *dat, int len, const uint8_t poly) {
  // cppcheck-suppress misra-c2012-7.2
  uint8_t crc = 0xFF;
  // cppcheck-suppress misra-c2012-12.3
  int i, j;
  for (i = len - 1; i >= 0; i--) {
    crc ^= dat[i];
    for (j = 0; j < 8; j++) {
      if ((crc & 0x80U) != 0U) {
        crc = (uint8_t)((crc << 1) ^ poly);
      }
      else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
