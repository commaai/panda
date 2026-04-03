#ifndef PANDA_BOARDS_UNUSED_FUNCS_H
#define PANDA_BOARDS_UNUSED_FUNCS_H

#include <stdbool.h>
#include <stdint.h>
#include "board_declarations.h"

void unused_set_fan_enabled(bool enabled);
void unused_set_siren(bool enabled);
void unused_set_bootkick(BootState state);
void unused_set_ir_power(uint8_t percentage);
bool unused_read_som_gpio(void);
void unused_set_amp_enabled(bool enabled);
uint32_t unused_read_voltage_mV(void);
uint32_t unused_read_current_mA(void);
int unused_comms_control_handler(ControlPacket_t *req, uint8_t *resp);

#endif
