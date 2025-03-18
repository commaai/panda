#pragma once
#include <stdint.h>
#include <stdbool.h>

extern const uint8_t gitversion[18];
extern int _app_start[0xc000]; // Only first 3 sectors of size 0x4000 are used

void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);
int get_health_pkt(void *dat);

