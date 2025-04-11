#pragma once
#include <stdint.h>
#include <stdbool.h>

extern int _app_start[0xc000]; // Only first 3 sectors of size 0x4000 are used

extern void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);
extern void set_safety_mode(uint16_t mode, uint16_t param);
extern bool is_car_safety_mode(uint16_t mode);

void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);
