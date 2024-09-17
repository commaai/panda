#pragma once

#include <stdint.h>
#include <stdbool.h>

extern bool bootkick_ign_prev;
extern BootState boot_state;
extern uint8_t bootkick_harness_status_prev;

extern uint8_t boot_reset_countdown;
extern uint8_t waiting_to_boot_countdown;
extern bool bootkick_reset_triggered;
extern uint16_t bootkick_last_serial_ptr;

void bootkick_tick(bool ignition, bool recent_heartbeat);
