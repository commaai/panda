#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board/config.h"

// ======================= BOOT STATE =======================

typedef enum {
  BOOT_STANDBY,
  BOOT_BOOTKICK,
  BOOT_RESET,
} BootState;

// ======================= BOARD FUNCTION TYPES =======================

typedef bool (*board_get_button)(void);
typedef bool (*board_read_som_gpio)(void);
typedef float (*board_get_channel_power)(uint8_t channel);
typedef uint16_t (*board_get_sbu_mV)(uint8_t channel, uint8_t sbu);
typedef uint32_t (*board_read_current_mA)(void);
typedef uint32_t (*board_read_voltage_mV)(void);
typedef void (*board_board_tick)(void);
typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);
typedef void (*board_enable_header_pin)(uint8_t pin_num, bool enabled);
typedef void (*board_init_bootloader)(void);
typedef void (*board_init)(void);
typedef void (*board_set_amp_enabled)(bool enabled);
typedef void (*board_set_bootkick)(BootState state);
typedef void (*board_set_can_mode)(uint8_t mode);
typedef void (*board_set_fan_enabled)(bool enabled);
typedef void (*board_set_harness_orientation)(uint8_t orientation);
typedef void (*board_set_ignition)(bool enabled);
typedef void (*board_set_individual_ignition)(uint8_t bitmask);
typedef void (*board_set_ir_power)(uint8_t percentage);
typedef void (*board_set_panda_individual_power)(uint8_t port_num, bool enabled);
typedef void (*board_set_panda_power)(bool enabled);
typedef void (*board_set_siren)(bool enabled);

// ======================= BOARD STRUCT =======================

typedef struct board {
  struct harness_configuration *harness_config;
  GPIO_TypeDef * const led_GPIO[3];
  const uint8_t led_pin[3];
  const uint8_t led_pwm_channels[3]; // leave at 0 to disable PWM
  const bool has_spi;
  const bool has_fan;
  const uint16_t avdd_mV;
  const uint8_t fan_enable_cooldown_time;
  board_init init;
  board_init_bootloader init_bootloader;
  board_board_tick board_tick;
  board_set_panda_power set_panda_power;
  board_get_button get_button;
  board_enable_can_transceiver enable_can_transceiver;
  board_set_can_mode set_can_mode;
  board_read_voltage_mV read_voltage_mV;
  board_read_current_mA read_current_mA;
  board_set_ir_power set_ir_power;
  board_set_fan_enabled set_fan_enabled;
  board_set_siren set_siren;
  board_set_bootkick set_bootkick;
  board_read_som_gpio read_som_gpio;
  board_set_amp_enabled set_amp_enabled;
  board_set_panda_individual_power set_panda_individual_power;
  board_set_ignition set_ignition;
  board_set_individual_ignition set_individual_ignition;
  board_set_harness_orientation set_harness_orientation;
  board_enable_header_pin enable_header_pin;
  board_get_channel_power get_channel_power;
  board_get_sbu_mV get_sbu_mV;
} board;

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
