
// TODO: this driver relies heavily on polling,
// if we want it to be more async, we should use interrupts

bool i2c_write_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t value) {
  // Setup transfer and send START + addr
  bool start_success = false;
  for(uint32_t i=0U; i<10U; i++) {
    register_clear_bits(&I2C->CR2, I2C_CR2_ADD10);
    I2C->CR2 = ((addr << 1U) & I2C_CR2_SADD_Msk);
    register_clear_bits(&I2C->CR2, I2C_CR2_RD_WRN);
    register_set_bits(&I2C->CR2, I2C_CR2_AUTOEND);
    I2C->CR2 |= (2 << I2C_CR2_NBYTES_Pos);

    I2C->CR2 |= I2C_CR2_START;
    while((I2C->CR2 & I2C_CR2_START) != 0U);

    // check if we lost arbitration
    if ((I2C->ISR & I2C_ISR_ARLO) != 0U) {
      register_set_bits(&I2C->ICR, I2C_ICR_ARLOCF);
    } else {
      start_success = true;
      break;
    }
  }

  if (!start_success) {
    return false;
  }

  // Send data
  while((I2C->ISR & I2C_ISR_TXIS) == 0U);
  I2C->TXDR = reg;
  while((I2C->ISR & I2C_ISR_TXIS) == 0U);
  I2C->TXDR = value;

  return true;
}

bool i2c_read_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t *value) {
  // Setup transfer and send START + addr
  bool start_success = false;
  for(uint32_t i=0U; i<10U; i++) {
    register_clear_bits(&I2C->CR2, I2C_CR2_ADD10);
    I2C->CR2 = ((addr << 1U) & I2C_CR2_SADD_Msk);
    register_clear_bits(&I2C->CR2, I2C_CR2_RD_WRN);
    register_clear_bits(&I2C->CR2, I2C_CR2_AUTOEND);
    I2C->CR2 |= (1 << I2C_CR2_NBYTES_Pos);

    I2C->CR2 |= I2C_CR2_START;
    while((I2C->CR2 & I2C_CR2_START) != 0U);

    // check if we lost arbitration
    if ((I2C->ISR & I2C_ISR_ARLO) != 0U) {
      register_set_bits(&I2C->ICR, I2C_ICR_ARLOCF);
    } else {
      start_success = true;
      break;
    }
  }

  if (!start_success) {
    return false;
  }

  // Send data
  while((I2C->ISR & I2C_ISR_TXIS) == 0U);
  I2C->TXDR = reg;

  // Restart
  I2C->CR2 = (((addr << 1U) | 0x1) & I2C_CR2_SADD_Msk) | (1 << I2C_CR2_NBYTES_Pos) | I2C_CR2_RD_WRN | I2C_CR2_START;
  while((I2C->CR2 & I2C_CR2_START) != 0U);

  // Read data
  while((I2C->ISR & I2C_ISR_RXNE) == 0U);
  *value = I2C->RXDR;

  // Stop
  I2C->CR2 |= I2C_CR2_STOP;

  return true;
}

bool i2c_set_reg_bits(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t bits) {
  uint8_t value;
  if(!i2c_read_reg(I2C, addr, reg, &value)) {
    return false;
  } 
  return i2c_write_reg(I2C, addr, reg, value | bits);
}

bool i2c_clear_reg_bits(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t bits) {
  uint8_t value;
  if(!i2c_read_reg(I2C, addr, reg, &value)) {
    return false;
  } 
  return i2c_write_reg(I2C, addr, reg, value & ~bits);
}

bool i2c_set_reg_mask(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t value, uint8_t mask) {
  uint8_t old_value;
  if(!i2c_read_reg(I2C, addr, reg, &old_value)) {
    return false;
  } 
  return i2c_write_reg(I2C, addr, reg, (old_value & ~mask) | (value & mask));
}

void i2c_init(I2C_TypeDef *I2C) {
  // 100kHz clock speed
  I2C->TIMINGR = 0x107075B0;
  I2C->CR1 = I2C_CR1_PE;
}