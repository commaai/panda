#include "board/drivers/drivers.h"

// Function declarations
void fan_set_power(uint8_t percentage);
void llfan_init(void);
void fan_init(void);
// Call this at FAN_TICK_FREQ
void fan_tick(void);
