#include <stdint.h>
#include <stdbool.h>

extern void print(const char *a);
void puth(unsigned int i);
extern uint32_t microsecond_timer_get(void);

#include "safety/board/utils.h"
#include "safety/board/faults_declarations.h"
#include "safety/board/can.h"
#include "safety/board/drivers/can_common.h"
#include "safety/safety.h"
