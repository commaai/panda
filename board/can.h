// cppcheck-suppress-file misra-c2012-2.3
#ifndef PANDA_CAN_H
#define PANDA_CAN_H
#include <stdbool.h>

#include "opendbc/safety/can.h"

#define PANDA_CAN_CNT 3U

#define BYTE_ARRAY_TO_WORD(w, arr) (w) = (((uint32_t)(arr)[0]) | ((uint32_t)(arr)[1] << 8U) | ((uint32_t)(arr)[2] << 16U) | ((uint32_t)(arr)[3] << 24U))
#define WORD_TO_BYTE_ARRAY(arr, w) (arr)[0] = (uint8_t)((w) & 0xFFU); (arr)[1] = (uint8_t)(((w) >> 8U) & 0xFFU); (arr)[2] = (uint8_t)(((w) >> 16U) & 0xFFU); (arr)[3] = (uint8_t)(((w) >> 24U) & 0xFFU)

typedef struct {
  uint32_t w_ptr;
  uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

#endif
