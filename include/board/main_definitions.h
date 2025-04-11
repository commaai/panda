#pragma once
#include <stdbool.h>
#include <stdint.h>

// ******************** Prototypes ********************
void print(const char *a);
void puth(unsigned int i);
void puth4(unsigned int i);
void hexdump(const void *a, int l);
typedef struct board board;
typedef struct harness_configuration harness_configuration;

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;
extern uint32_t uptime_cnt;
extern bool green_led_enabled;

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;            // set over USB

// siren state
extern bool siren_enabled;
