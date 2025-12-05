#pragma once

#include "board/globals.h"
#include "board/comms.h"

void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);