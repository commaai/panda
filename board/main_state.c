#include <stdbool.h>
#include <stdint.h>

#ifdef STM32H7
#include "stm32h7xx.h"
#else
typedef struct TIM_TypeDef TIM_TypeDef;
#endif

#include "board/main_declarations.h"

// ********************* Globals **********************
uint8_t hw_type = 0;
board *current_board;
uint32_t uptime_cnt = 0;

// heartbeat state
uint32_t heartbeat_counter = 0;
bool heartbeat_lost = false;
bool heartbeat_disabled = false;            // set over USB

// siren state
bool siren_enabled = false;
