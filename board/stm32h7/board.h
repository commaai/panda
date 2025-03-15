#pragma once
// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "boards/board_declarations.h"
#include "boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "stm32h7/lladc.h"
#include "drivers/harness.h"
#include "drivers/fan.h"
#include "stm32h7/llfan.h"
#include "stm32h7/lldac.h"
#include "drivers/fake_siren.h"
#include "stm32h7/sound.h"
#include "drivers/clock_source.h"


void detect_board_type(void);