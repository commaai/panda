#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "panda.h"
#include "libc.h"
#include "can_definitions.h"

// from config.h
#define MIN(a,b)                                \
  ({ __typeof__ (a) _a = (a);                   \
    __typeof__ (b) _b = (b);                    \
    _a < _b ? _a : _b; })

#define MAX(a,b)                                \
  ({ __typeof__ (a) _a = (a);                   \
    __typeof__ (b) _b = (b);                    \
    _a > _b ? _a : _b; })

#define ABS(a)                                  \
 ({ __typeof__ (a) _a = (a);                    \
   (_a > 0) ? _a : (-_a); })

#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0
#define UNUSED(x) 0

void usb_cb_ep3_out_complete();
bool bitbang_gmlan(CANPacket_t *to_bang) { return true; }

#include "safety_declarations.h"
#include "drivers/can_common.h"

#define CANFD

#define COMPILE_TIME_ASSERT(pred) ((void)sizeof(char[1 - (2 * ((int)(!(pred))))]))
#define POWER_SAVE_STATUS_DISABLED 0
#define POWER_SAVE_STATUS_ENABLED 1

#define PROTOCOL_TESTS

#include "comms_definitions.h"
#include "main_comms.h"

