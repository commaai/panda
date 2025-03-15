#pragma once
// TODO: this driver relies heavily on polling,
// if we want it to be more async, we should use interrupts

bool i2c_set_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);

bool i2c_clear_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);

bool i2c_set_reg_mask(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t value, uint8_t mask);

void i2c_init(I2C_TypeDef *I2C);
