#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <boards/board.h>

void unused_init_bootloader(void);
void unused_set_ir_power(uint8_t percentage);
void unused_set_fan_enabled(bool enabled);
void unused_set_siren(bool enabled);
uint32_t unused_read_current(void);
void unused_set_bootkick(BootState state);
bool unused_read_som_gpio(void);
void unused_set_amp_enabled(bool enabled);
