#pragma once

#ifdef STM32H7
#include "stm32h7/stm32h7_config.h"
#elif defined(STM32F4)
#include "stm32f4/stm32f4_config.h"
#else
// TODO: uncomment this, cppcheck complains
// building for tests
//#include "fake_stm.h"
#endif