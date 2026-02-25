#ifndef BOOTSTUB
#include <stdint.h>
#include <stdbool.h>

typedef struct board board;

uint8_t hw_type = 0;
board *current_board;
uint32_t uptime_cnt = 0;

uint32_t heartbeat_counter = 0;
bool heartbeat_lost = false;
bool heartbeat_disabled = false;

bool siren_enabled = false;

#ifdef PANDA_JUNGLE
uint8_t harness_orientation = 0U;
uint8_t can_mode = 0U;
uint8_t ignition = 0U;
#endif
#endif
