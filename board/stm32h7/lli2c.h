#pragma once

// TODO: this driver relies heavily on polling,
// if we want it to be more async, we should use interrupts

#define I2C_RETRY_COUNT 10U
#define I2C_TIMEOUT_US 100000U

bool i2c_status_wait(const volatile uint32_t *reg, uint32_t mask, uint32_t val);
void i2c_reset(I2C_TypeDef *I2C);
bool i2c_write_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t value);
bool i2c_read_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t *value);
bool i2c_set_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);
bool i2c_clear_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);
bool i2c_set_reg_mask(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t value, uint8_t mask);
void i2c_init(I2C_TypeDef *I2C);
