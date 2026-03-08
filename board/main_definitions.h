#include "main_declarations.h"

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;
extern uint32_t uptime_cnt;

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

// siren state
extern bool siren_enabled;
