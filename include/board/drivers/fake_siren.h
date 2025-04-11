#pragma once
#include "stm32h7/lli2c.h"

#define CODEC_I2C_ADDR 0x10

void fake_siren_init(void);
void fake_siren_codec_enable(bool enabled);
void fake_siren_set(bool enabled);
