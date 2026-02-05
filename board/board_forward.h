#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board/boards/boards.h"

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