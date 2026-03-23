#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "board/drivers/drivers.h"

uint8_t crc_checksum(const uint8_t *dat, int len, const uint8_t poly) {
  uint8_t crc = 0xFFU;
  int i;
  int j;
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
