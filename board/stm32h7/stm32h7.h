#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx.h"
#include "board/stm32h7/lladc_declarations.h"
#include "board/stm32h7/llfdcan_declarations.h"
#include "board/stm32h7/llusb_declarations.h"

// There are 163 external interrupt sources (see stm32f735xx.h)
#define NUM_INTERRUPTS 163U

// lladc
void adc_init(ADC_TypeDef *adc);
uint16_t adc_get_raw(const adc_signal_t *signal);
uint16_t adc_get_mV(const adc_signal_t *signal);

// lldts
void dts_init(void);
float dts_get_temperature(void);

// llfan
void llfan_init(void);

// llflash
bool flash_is_locked(void);
void flash_unlock(void);
bool flash_erase_sector(uint8_t sector, bool unlocked);
void flash_write_word(void *prog_ptr, uint32_t data);
void flush_write_buffer(void);

// lli2c
bool i2c_set_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);
bool i2c_clear_reg_bits(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t bits);
bool i2c_set_reg_mask(I2C_TypeDef *I2C, uint8_t address, uint8_t regis, uint8_t value, uint8_t mask);
void i2c_init(I2C_TypeDef *I2C);

// llspi
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(const uint8_t *addr, int len);
void llspi_init(void);

// clock
void clock_init(void);

// peripherals
#ifdef BOOTSTUB
void gpio_usb_init(void);
#endif
void gpio_spi_init(void);
void gpio_usart2_init(void);
void gpio_uart7_init(void);
void common_init_gpio(void);
void flasher_peripherals_init(void);
void peripherals_init(void);
void enable_interrupt_timer(void);

void early_gpio_float(void);

// sound
extern uint16_t sound_output_level;
void sound_tick(void);
void sound_init_dac(void);
void sound_stop_dac(void);
void sound_init(void);
