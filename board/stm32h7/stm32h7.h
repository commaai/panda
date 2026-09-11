#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "board/stm32h7/stm32h7_config.h"

typedef enum {
  SAMPLETIME_1_CYCLE = 0,
  SAMPLETIME_2_CYCLES = 1,
  SAMPLETIME_8_CYCLES = 2,
  SAMPLETIME_16_CYCLES = 3,
  SAMPLETIME_32_CYCLES = 4,
  SAMPLETIME_64_CYCLES = 5,
  SAMPLETIME_387_CYCLES = 6,
  SAMPLETIME_810_CYCLES = 7
} adc_sample_time_t;

typedef enum {
  OVERSAMPLING_1 = 0,
  OVERSAMPLING_2 = 1,
  OVERSAMPLING_4 = 2,
  OVERSAMPLING_8 = 3,
  OVERSAMPLING_16 = 4,
  OVERSAMPLING_32 = 5,
  OVERSAMPLING_64 = 6,
  OVERSAMPLING_128 = 7,
  OVERSAMPLING_256 = 8,
  OVERSAMPLING_512 = 9,
  OVERSAMPLING_1024 = 10
} adc_oversampling_t;

typedef struct {
  ADC_TypeDef *adc;
  uint8_t channel;
  adc_sample_time_t sample_time;
  adc_oversampling_t oversampling;
} adc_signal_t;

#define ADC_CHANNEL_DEFAULT(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_32_CYCLES, .oversampling = OVERSAMPLING_64}

// FDCAN core settings
#define FDCAN_START_ADDRESS 0x4000AC00UL
#define FDCAN_OFFSET 3384UL // bytes for each FDCAN module, equally

// FDCAN_RX_FIFO_0_EL_CNT + FDCAN_TX_FIFO_EL_CNT can't exceed 47 elements (47 * 72 bytes = 3,384 bytes) per FDCAN module

// RX FIFO 0
#define FDCAN_RX_FIFO_0_EL_CNT 46UL
#define FDCAN_RX_FIFO_0_HEAD_SIZE 8UL // bytes
#define FDCAN_RX_FIFO_0_DATA_SIZE 64UL // bytes
#define FDCAN_RX_FIFO_0_EL_SIZE (FDCAN_RX_FIFO_0_HEAD_SIZE + FDCAN_RX_FIFO_0_DATA_SIZE)

// TX FIFO
#define FDCAN_TX_FIFO_EL_CNT 1UL
#define FDCAN_TX_FIFO_HEAD_SIZE 8UL // bytes
#define FDCAN_TX_FIFO_DATA_SIZE 64UL // bytes
#define FDCAN_TX_FIFO_EL_SIZE (FDCAN_TX_FIFO_HEAD_SIZE + FDCAN_TX_FIFO_DATA_SIZE)

// kbps multiplied by 10
#define SPEEDS_ARRAY_SIZE 8
extern const uint32_t speeds[SPEEDS_ARRAY_SIZE];
#define DATA_SPEEDS_ARRAY_SIZE 10
extern const uint32_t data_speeds[DATA_SPEEDS_ARRAY_SIZE];

bool llcan_set_speed(FDCAN_GlobalTypeDef *FDCANx, uint32_t speed, uint32_t data_speed, bool non_iso, bool loopback, bool silent);
void llcan_irq_disable(const FDCAN_GlobalTypeDef *FDCANx);
void llcan_irq_enable(const FDCAN_GlobalTypeDef *FDCANx);
bool llcan_init(FDCAN_GlobalTypeDef *FDCANx);
void llcan_clear_send(FDCAN_GlobalTypeDef *FDCANx);

extern USB_OTG_GlobalTypeDef *USBx;

#define USBx_DEVICE     ((USB_OTG_DeviceTypeDef *)((uint32_t)USBx + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)    ((USB_OTG_INEndpointTypeDef *)((uint32_t)USBx + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)   ((USB_OTG_OUTEndpointTypeDef *)((uint32_t)USBx + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_DFIFO(i)   *(__IO uint32_t *)((uint32_t)USBx + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))

void usb_init(void);

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
bool i2c_write_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t value);
bool i2c_read_reg(I2C_TypeDef *I2C, uint8_t addr, uint8_t reg, uint8_t *value);
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
