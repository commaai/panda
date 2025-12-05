#pragma once

#include <stdbool.h>

#define CODEC_I2C_ADDR 0x10

void fake_i2c_siren_set(bool enabled);
void fake_siren_set(bool enabled);