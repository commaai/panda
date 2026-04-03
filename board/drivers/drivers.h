#pragma once

#include "board/config.h"
#include "board/utils.h"
#include "board/drivers/driver_declarations.h"
#include "board/can.h"
#include "board/drivers/can_common.h"
#include "board/health.h"
#include "board/crc.h"

// Common driver headers
#include "board/drivers/bootkick.h"
#include "board/drivers/interrupts.h"
#include "board/drivers/registers.h"
#include "board/drivers/simple_watchdog.h"
#include "board/drivers/spi.h"
#include "board/drivers/uart.h"
#include "board/drivers/usb.h"
#include "board/drivers/gpio.h"
#include "board/drivers/pwm.h"
#include "board/drivers/led.h"
#include "board/drivers/fan.h"
#include "board/drivers/harness.h"
#include "board/drivers/fake_siren.h"
#include "board/drivers/clock_source.h"

#ifdef STM32H7
#include "board/stm32h7/lladc_declarations.h"
#include "board/stm32h7/sound.h"
#endif
