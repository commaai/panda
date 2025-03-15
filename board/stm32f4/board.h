#pragma once
// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/board_declarations.h"
#include "boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "stm32f4/lladc.h"
#include "drivers/harness.h"
#include "drivers/fan.h"
#include "stm32f4/llfan.h"
#include "drivers/clock_source.h"

// Unused functions on F4
void sound_tick(void);

void detect_board_type(void);
