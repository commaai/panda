#pragma once

#include "board/comms_definitions.h"
#include "board/jungle/jungle_health.h"

extern int _app_start[0xc000];
extern bool generated_can_traffic;

int get_jungle_health_pkt(void *dat);
void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);
