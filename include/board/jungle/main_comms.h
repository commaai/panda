#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "comms_definitions.h"

extern int _app_start[0xc000]; // Only first 3 sectors of size 0x4000 are used

extern bool generated_can_traffic;
int get_jungle_health_pkt(void *dat);
void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);
extern void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);
