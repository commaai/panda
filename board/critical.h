#include <stdint.h>
#include <stdbool.h>

#include "critical_declarations.h"

// ********************* Critical section helpers *********************
extern uint8_t global_critical_depth;
static volatile bool interrupts_enabled;

void enable_interrupts(void);
void disable_interrupts(void);
