#pragma once

#include <stdint.h>

#include "board/can.h"

void body_can_init(void);
void body_can_periodic(uint32_t now);
void body_can_safety_rx(const CANPacket_t *msg);
