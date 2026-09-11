#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "board/stm32h7/stm32h7_config.h"
#include "board/stm32h7/lladc_declarations.h"
#include "board/stm32h7/llfdcan_declarations.h"
#include "board/stm32h7/llusb_declarations.h"

void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(const uint8_t *addr, int len);
