#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "platform_definitions.h"

void i2c_reset(I2C_TypeDef *I2C);
bool i2c_write_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t value);
bool i2c_read_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t *value);
bool i2c_set_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);
bool i2c_clear_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);
bool i2c_set_reg_mask(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t value, uint8_t mask);
void i2c_init(I2C_TypeDef *I2C);
