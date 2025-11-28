#pragma once

#include <stdbool.h>

#define CODEC_I2C_ADDR 0x10

void siren_tim7_init(void);
void siren_dac_init(void);
void siren_dma_init(void);
void fake_siren_codec_enable(bool enabled);
void fake_i2c_siren_set(bool enabled);
void fake_siren_set(bool enabled);