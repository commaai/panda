#pragma once

#include "board/utils.h"
#ifdef STM32H7
#include "board/stm32h7/stm32h7.h"
#endif
#include "board/drivers/drivers.h"
#include "board/sys/sys.h"
#include "board/main_declarations.h"
#include "board/comms_definitions.h"
#ifdef STM32H7
#include "board/obj/gitversion.h"
#endif

#ifdef PANDA_JUNGLE
#include "board/jungle/boards/board_declarations.h"
#include "board/jungle/jungle_health.h"
#elif defined(PANDA_BODY)
#include "board/body/boards/board_declarations.h"
#include "board/body/body.h"
#else
#include "board/boards/board_declarations.h"
#endif

#ifndef BOOTSTUB
#include "opendbc/safety/declarations.h"
#endif

// Config includes implementations, so load it after the shared declarations.
#include "board/config.h"
