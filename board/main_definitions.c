#include "board/config.h"
#include "board/main_declarations.h"

// ********************* Globals **********************
uint8_t hw_type = 0;
const board *current_board;
uint32_t uptime_cnt = 0;
uint32_t enter_bootloader_mode = 0;

// heartbeat state
uint32_t heartbeat_counter = 0;
bool heartbeat_lost = false;
bool heartbeat_disabled = false;            // set over USB

// siren state
bool siren_enabled = false;
uint32_t siren_countdown = 0U;
