#pragma once

#include "board/can_declarations.h"

// Stub header for safety.h to allow build to complete
// This is a temporary file to satisfy the include dependency

// Safety mode constants
#define SAFETY_SILENT 0xFFFF
#define SAFETY_NOOUTPUT 0x17
#define SAFETY_ALLOUTPUT 0x1337
#define SAFETY_ELM327 0xE1A

// Add minimal safety-related definitions needed for build
typedef struct {
  int dummy;
} safety_config_t;

// Add function stubs if needed by main.c
// These can be empty implementations for build purposes

// Safety hook function declarations
int safety_tx_hook(CANPacket_t *to_send);
int safety_fwd_hook(int bus_number, int addr);
int safety_rx_hook(CANPacket_t *to_push);
int set_safety_hooks(uint16_t mode, uint16_t param);
void safety_tick(safety_config_t *config);

// Safety variables
extern bool controls_allowed;
extern uint32_t current_safety_mode;
extern uint16_t current_safety_param;
extern uint32_t alternative_experience;
extern uint32_t safety_rx_checks_invalid;
extern bool heartbeat_engaged;
extern uint32_t heartbeat_engaged_mismatches;
extern uint32_t safety_mode_cnt;
extern safety_config_t current_safety_config;