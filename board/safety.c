// Until opendbc is properly converted to source/header format, we compile our
// own bespoke safety object file so that we can link opendbc implementations
// into panda.
#include <stdint.h>
#include <stdbool.h>

extern void print(const char *a);
extern void puth(unsigned int i);
extern uint32_t microsecond_timer_get(void);

// Suppress funky header inclusion ordering. We need the forward declarations
// above. This will all eventually be deleted when opendbc is fixed.
// cppcheck-suppress-begin misra-c2012-20.1 -
#include "safety/board/utils.h"
#include "safety/board/faults.h"
#include "safety/board/can.h"
#include "safety/board/drivers/can_common.h"
#include "safety/safety.h"
// cppcheck-suppress-end misra-c2012-20.1
