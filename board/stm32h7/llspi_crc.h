#pragma once

// This peripheral has no other application users. SPI calls are serialized in
// the CS handler. Keep this ownership rule if another CRC user is introduced.
// IEEE CRC32: byte input reflection, reflected output, init/final XOR all ones.
static uint32_t llspi_crc32_hw(const uint8_t *data, uint32_t len) {
  CRC->CR = CRC_CR_REV_IN_0 | CRC_CR_REV_OUT | CRC_CR_RESET;
  // Byte reversal within each input word does not reorder the bytes. Feed the
  // earliest byte in bits31:24 for word accesses; byte accesses need no swap.
  uint32_t offset = 0U;
  while ((len - offset) >= 4U) {
    CRC->DR = __REV(__UNALIGNED_UINT32_READ(&data[offset]));
    offset += 4U;
  }
  while (offset < len) {
    *((volatile uint8_t *)&CRC->DR) = data[offset];
    offset++;
  }
  return CRC->DR ^ 0xFFFFFFFFU;
}

static bool llspi_crc_init(void) {
  register_set_bits(&(RCC->AHB4ENR), RCC_AHB4ENR_CRCEN);
  __DSB();
  register_set(&(CRC->POL), 0x04C11DB7U, 0xFFFFFFFFU);
  register_set(&(CRC->INIT), 0xFFFFFFFFU, 0xFFFFFFFFU);

  // Refuse sessions if the peripheral does not produce the IEEE CRC32 value.
  static const uint8_t check[] __attribute__((aligned(4))) = "123456789";
  return llspi_crc32_hw(check, sizeof(check) - 1U) == 0xCBF43926U;
}
