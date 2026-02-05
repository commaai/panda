#pragma once

#include <stdint.h>
#include <stdbool.h>

// ======================= BOOT STATE =======================
// Must be defined before board_struct.h which uses BootState

typedef enum {
  BOOT_STANDBY,
  BOOT_BOOTKICK,
  BOOT_RESET,
} BootState;

#include "board/board_struct.h"

// ======================= UNUSED FUNCS =======================

void unused_init_bootloader(void);
void unused_set_ir_power(uint8_t percentage);
void unused_set_fan_enabled(bool enabled);
void unused_set_siren(bool enabled);
uint32_t unused_read_current(void);
void unused_set_bootkick(BootState state);
bool unused_read_som_gpio(void);
void unused_set_amp_enabled(bool enabled);

// ======================= RED =======================
// Red Panda (STM32H7) + Harness

struct harness_configuration;
struct board;

extern struct harness_configuration red_harness_config;
extern struct board board_red;

uint32_t red_read_voltage_mV(void);

// ======================= TRES =======================

extern struct board board_tres;

void tres_set_can_mode(uint8_t mode);
bool tres_read_som_gpio(void);

// ======================= CUATRO =======================
// Cuatro (STM32H7) + Harness

extern struct board board_cuatro;
