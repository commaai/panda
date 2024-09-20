#pragma once
#include "can_declarations.h"

const uint8_t PANDA_CAN_CNT = 3U;
const uint8_t PANDA_BUS_CNT = 3U;

const unsigned char dlc_to_len[DLC_TO_LEN_ARRAY_SIZE] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};
