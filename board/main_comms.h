#pragma once

#include "comms.h"

// Prototypes
void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);

// send on serial, first byte to select the ring
void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);