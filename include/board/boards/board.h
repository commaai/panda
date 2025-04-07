#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "platform_definitions.h"

#ifdef PANDA_JUNGLE
#include "jungle/boards/board.h"
#else

typedef enum {
  BOOT_STANDBY,
  BOOT_BOOTKICK,
  BOOT_RESET,
} BootState;

// ******************** Prototypes ********************

typedef void (*board_init)(void);
typedef void (*board_init_bootloader)(void);
typedef void (*board_enable_can_transceiver)(uint8_t transceiver, bool enabled);
typedef void (*board_set_can_mode)(uint8_t mode);
typedef bool (*board_check_ignition)(void);
typedef uint32_t (*board_read_voltage_mV)(void);
typedef uint32_t (*board_read_current_mA)(void);
typedef void (*board_set_ir_power)(uint8_t percentage);
typedef void (*board_set_fan_enabled)(bool enabled);
typedef void (*board_set_siren)(bool enabled);
typedef void (*board_set_bootkick)(BootState state);
typedef bool (*board_read_som_gpio)(void);
typedef void (*board_set_amp_enabled)(bool enabled);

typedef struct harness_configuration harness_configuration; // Forward decl.
typedef struct board {
  harness_configuration *harness_config;
  GPIO_TypeDef * const led_GPIO[3];
  const uint8_t led_pin[3];
  const bool has_spi;
  const bool has_canfd;
  const uint16_t fan_max_rpm;
  const uint16_t avdd_mV;
  const bool fan_stall_recovery;
  const uint8_t fan_enable_cooldown_time;
  const uint8_t fan_max_pwm;
  board_init init;
  board_init_bootloader init_bootloader;
  board_enable_can_transceiver enable_can_transceiver;
  board_set_can_mode set_can_mode;
  board_check_ignition check_ignition;
  board_read_voltage_mV read_voltage_mV;
  board_read_current_mA read_current_mA;
  board_set_ir_power set_ir_power;
  board_set_fan_enabled set_fan_enabled;
  board_set_siren set_siren;
  board_set_bootkick set_bootkick;
  board_read_som_gpio read_som_gpio;
  board_set_amp_enabled set_amp_enabled;
} board;

// ******************* Definitions ********************
// These should match the enums in cereal/log.capnp and __init__.py
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_WHITE_PANDA 1U
#define HW_TYPE_GREY_PANDA 2U
#define HW_TYPE_BLACK_PANDA 3U
#define HW_TYPE_PEDAL 4U
#define HW_TYPE_UNO 5U
#define HW_TYPE_DOS 6U
#define HW_TYPE_RED_PANDA 7U
#define HW_TYPE_RED_PANDA_V2 8U
#define HW_TYPE_TRES 9U
#define HW_TYPE_CUATRO 10U

// USB power modes (from cereal.log.health)
#define USB_POWER_NONE 0U
#define USB_POWER_CLIENT 1U
#define USB_POWER_CDP 2U
#define USB_POWER_DCP 3U

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_OBD_CAN2 1U

extern struct board board_black;
extern struct board board_dos;
extern struct board board_uno;
extern struct board board_tres;
extern struct board board_grey;
extern struct board board_white;
extern struct board board_cuatro;
extern struct board board_red;
#endif
