#pragma once

// Delegate CAN packet definition (CANPacket_t, dlc_to_len, GET_BUS/LEN/ADDR,
// CANPACKET_HEAD_SIZE, CANPACKET_DATA_SIZE_MAX) to opendbc/safety/can.h.
#include "opendbc/safety/can.h"

#if !defined(STM32F4)
  #define CANFD
#endif
