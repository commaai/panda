#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "stm32h7_config.h"

// ======================= CLOCK =======================

typedef enum {
  PACKAGE_UNKNOWN = 0,
  PACKAGE_WITH_SMPS = 1,
  PACKAGE_WITHOUT_SMPS = 2,
} PackageSMPSType;

// TODO: find a better way to distinguish between H725 (using SMPS) and H723 (lacking SMPS)
// The package will do for now, since we have only used TFBGA100 for H723

void clock_init(void);

// ======================= PERIPHERALS =======================

void gpio_usb_init(void);
void gpio_spi_init(void);

#ifdef BOOTSTUB
void gpio_usart2_init(void);
#endif

void gpio_uart7_init(void);

// Common GPIO initialization
void common_init_gpio(void);

#ifdef BOOTSTUB
void flasher_peripherals_init(void);
#endif

// Peripheral initialization
void peripherals_init(void);

void enable_interrupt_timer(void);

// ======================= LLFLASH =======================

bool flash_is_locked(void);
void flash_unlock(void);
bool flash_erase_sector(uint8_t sector, bool unlocked);
void flash_write_word(void *prog_ptr, uint32_t data);
void flush_write_buffer(void);

// ======================= LLUART =======================

struct uart_ring;
// This read after reading ISR clears all error interrupts. We don't want compiler warnings, nor optimizations
#define UART_READ_RDR(uart) volatile uint8_t t = (uart)->RDR; UNUSED(t);

void uart_init(struct uart_ring *q, unsigned int baud);

// ======================= LLSPI =======================

// master -> panda DMA start
void llspi_mosi_dma(uint8_t *addr, int len);
// panda -> master DMA start
void llspi_miso_dma(uint8_t *addr, int len);
void llspi_init(void);

// ======================= LLI2C =======================

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

// ======================= LLFDCAN =======================

// SAE J2284-4 document specifies a bus-line network running at 2 Mbit/s
// SAE J2284-5 document specifies a point-to-point communication running at 5 Mbit/s

#define CAN_PCLK 80000U // KHz, sourced from PLL1Q
#define BITRATE_PRESCALER 2U // Valid from 250Kbps to 5Mbps with 80Mhz clock
#define CAN_SP_NOMINAL 80U // 80% for both SAE J2284-4 and SAE J2284-5
#define CAN_SP_DATA_2M 80U // 80% for SAE J2284-4
#define CAN_SP_DATA_5M 75U // 75% for SAE J2284-5
#define CAN_QUANTA(speed, prescaler) (CAN_PCLK / ((speed) / 10U * (prescaler)))
#define CAN_SEG1(tq, sp) (((tq) * (sp) / 100U)- 1U)
#define CAN_SEG2(tq, sp) ((tq) * (100U - (sp)) / 100U)

// FDCAN core settings
#define FDCAN_START_ADDRESS 0x4000AC00UL
#define FDCAN_OFFSET 3384UL // bytes for each FDCAN module, equally
#define FDCAN_OFFSET_W 846UL // words for each FDCAN module, equally

// FDCAN_RX_FIFO_0_EL_CNT + FDCAN_TX_FIFO_EL_CNT can't exceed 47 elements (47 * 72 bytes = 3,384 bytes) per FDCAN module

// RX FIFO 0
#define FDCAN_RX_FIFO_0_EL_CNT 46UL
#define FDCAN_RX_FIFO_0_HEAD_SIZE 8UL // bytes
#define FDCAN_RX_FIFO_0_DATA_SIZE 64UL // bytes
#define FDCAN_RX_FIFO_0_EL_SIZE (FDCAN_RX_FIFO_0_HEAD_SIZE + FDCAN_RX_FIFO_0_DATA_SIZE)
#define FDCAN_RX_FIFO_0_EL_W_SIZE (FDCAN_RX_FIFO_0_EL_SIZE / 4UL)
#define FDCAN_RX_FIFO_0_OFFSET 0UL

// TX FIFO
#define FDCAN_TX_FIFO_EL_CNT 1UL
#define FDCAN_TX_FIFO_HEAD_SIZE 8UL // bytes
#define FDCAN_TX_FIFO_DATA_SIZE 64UL // bytes
#define FDCAN_TX_FIFO_EL_SIZE (FDCAN_TX_FIFO_HEAD_SIZE + FDCAN_TX_FIFO_DATA_SIZE)
#define FDCAN_TX_FIFO_OFFSET (FDCAN_RX_FIFO_0_OFFSET + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_W_SIZE))

#define CAN_NAME_FROM_CANIF(CAN_DEV) (((CAN_DEV)==FDCAN1) ? "FDCAN1" : (((CAN_DEV) == FDCAN2) ? "FDCAN2" : "FDCAN3"))
#define CAN_NUM_FROM_CANIF(CAN_DEV) (((CAN_DEV)==FDCAN1) ? 0UL : (((CAN_DEV) == FDCAN2) ? 1UL : 2UL))

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

// ======================= LLUSB =======================

extern USB_OTG_GlobalTypeDef *USBx;

#define USBx_DEVICE     ((USB_OTG_DeviceTypeDef *)((uint32_t)USBx + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)    ((USB_OTG_INEndpointTypeDef *)((uint32_t)USBx + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)   ((USB_OTG_OUTEndpointTypeDef *)((uint32_t)USBx + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_DFIFO(i)   *(__IO uint32_t *)((uint32_t)USBx + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))
#define USBx_PCGCCTL    *(__IO uint32_t *)((uint32_t)USBx + USB_OTG_PCGCCTL_BASE)

#define USBD_FS_TRDT_VALUE        6UL
#define USB_OTG_SPEED_FULL        3U
#define DCFG_FRAME_INTERVAL_80    0U

void usb_irqhandler(void);
void usb_init(void);

// ======================= LLFAN =======================

void llfan_init(void);

// ======================= SOUND =======================

#define SOUND_RX_BUF_SIZE 1000U
#define SOUND_TX_BUF_SIZE (SOUND_RX_BUF_SIZE/2U)
#define MIC_RX_BUF_SIZE 512U
#define MIC_TX_BUF_SIZE (MIC_RX_BUF_SIZE * 2U)
#define SOUND_IDLE_TIMEOUT 4U
#define MIC_SKIP_BUFFERS 2U // Skip first 2 buffers (1024 samples = ~21ms at 48kHz)

void sound_tick(void);
void sound_init_dac(void);
void sound_init(void);
void sound_stop_dac(void);

// ======================= INTERRUPT HANDLERS =======================
// Bare interrupt handlers for STM32H7x5

void WWDG_IRQHandler(void);
void PVD_AVD_IRQHandler(void);
void TAMP_STAMP_IRQHandler(void);
void RTC_WKUP_IRQHandler(void);
void FLASH_IRQHandler(void);
void RCC_IRQHandler(void);
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI2_IRQHandler(void);
void EXTI3_IRQHandler(void);
void EXTI4_IRQHandler(void);
void DMA1_Stream0_IRQHandler(void);
void DMA1_Stream1_IRQHandler(void);
void DMA1_Stream2_IRQHandler(void);
void DMA1_Stream3_IRQHandler(void);
void DMA1_Stream4_IRQHandler(void);
void DMA1_Stream5_IRQHandler(void);
void DMA1_Stream6_IRQHandler(void);
void ADC_IRQHandler(void);
void FDCAN1_IT0_IRQHandler(void);
void FDCAN2_IT0_IRQHandler(void);
void FDCAN1_IT1_IRQHandler(void);
void FDCAN2_IT1_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
void TIM1_BRK_IRQHandler(void);
void TIM1_UP_TIM10_IRQHandler(void);
void TIM1_TRG_COM_IRQHandler(void);
void TIM1_CC_IRQHandler(void);
void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);
void TIM4_IRQHandler(void);
void I2C1_EV_IRQHandler(void);
void I2C1_ER_IRQHandler(void);
void I2C2_EV_IRQHandler(void);
void I2C2_ER_IRQHandler(void);
void SPI1_IRQHandler(void);
void SPI2_IRQHandler(void);
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void USART3_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void RTC_Alarm_IRQHandler(void);
void TIM8_BRK_TIM12_IRQHandler(void);
void TIM8_UP_TIM13_IRQHandler(void);
void TIM8_TRG_COM_TIM14_IRQHandler(void);
void TIM8_CC_IRQHandler(void);
void DMA1_Stream7_IRQHandler(void);
void FMC_IRQHandler(void);
void SDMMC1_IRQHandler(void);
void TIM5_IRQHandler(void);
void SPI3_IRQHandler(void);
void UART4_IRQHandler(void);
void UART5_IRQHandler(void);
void TIM6_DAC_IRQHandler(void);
void TIM7_IRQHandler(void);
void DMA2_Stream0_IRQHandler(void);
void DMA2_Stream1_IRQHandler(void);
void DMA2_Stream2_IRQHandler(void);
void DMA2_Stream3_IRQHandler(void);
void DMA2_Stream4_IRQHandler(void);
void ETH_IRQHandler(void);
void ETH_WKUP_IRQHandler(void);
void FDCAN_CAL_IRQHandler(void);
void DMA2_Stream5_IRQHandler(void);
void DMA2_Stream6_IRQHandler(void);
void DMA2_Stream7_IRQHandler(void);
void USART6_IRQHandler(void);
void I2C3_EV_IRQHandler(void);
void I2C3_ER_IRQHandler(void);
void OTG_HS_EP1_OUT_IRQHandler(void);
void OTG_HS_EP1_IN_IRQHandler(void);
void OTG_HS_WKUP_IRQHandler(void);
void OTG_HS_IRQHandler(void);
void DCMI_PSSI_IRQHandler(void);
void CRYP_IRQHandler(void);
void HASH_RNG_IRQHandler(void);
void FPU_IRQHandler(void);
void UART7_IRQHandler(void);
void UART8_IRQHandler(void);
void SPI4_IRQHandler(void);
void SPI5_IRQHandler(void);
void SPI6_IRQHandler(void);
void SAI1_IRQHandler(void);
void LTDC_IRQHandler(void);
void LTDC_ER_IRQHandler(void);
void DMA2D_IRQHandler(void);
void OCTOSPI1_IRQHandler(void);
void LPTIM1_IRQHandler(void);
void CEC_IRQHandler(void);
void I2C4_EV_IRQHandler(void);
void I2C4_ER_IRQHandler(void);
void SPDIF_RX_IRQHandler(void);
void DMAMUX1_OVR_IRQHandler(void);
void DFSDM1_FLT0_IRQHandler(void);
void DFSDM1_FLT1_IRQHandler(void);
void DFSDM1_FLT2_IRQHandler(void);
void DFSDM1_FLT3_IRQHandler(void);
void SWPMI1_IRQHandler(void);
void TIM15_IRQHandler(void);
void TIM16_IRQHandler(void);
void TIM17_IRQHandler(void);
void MDIOS_WKUP_IRQHandler(void);
void MDIOS_IRQHandler(void);
void MDMA_IRQHandler(void);
void SDMMC2_IRQHandler(void);
void HSEM1_IRQHandler(void);
void ADC3_IRQHandler(void);
void DMAMUX2_OVR_IRQHandler(void);
void BDMA_Channel0_IRQHandler(void);
void BDMA_Channel1_IRQHandler(void);
void BDMA_Channel2_IRQHandler(void);
void BDMA_Channel3_IRQHandler(void);
void BDMA_Channel4_IRQHandler(void);
void BDMA_Channel5_IRQHandler(void);
void BDMA_Channel6_IRQHandler(void);
void BDMA_Channel7_IRQHandler(void);
void COMP_IRQHandler(void);
void LPTIM2_IRQHandler(void);
void LPTIM3_IRQHandler(void);
void LPTIM4_IRQHandler(void);
void LPTIM5_IRQHandler(void);
void LPUART1_IRQHandler(void);
void CRS_IRQHandler(void);
void ECC_IRQHandler(void);
void SAI4_IRQHandler(void);
void DTS_IRQHandler(void);
void WAKEUP_PIN_IRQHandler(void);
void OCTOSPI2_IRQHandler(void);
void OTFDEC1_IRQHandler(void);
void OTFDEC2_IRQHandler(void);
void FMAC_IRQHandler(void);
void CORDIC_IRQHandler(void);
void UART9_IRQHandler(void);
void USART10_IRQHandler(void);
void I2C5_EV_IRQHandler(void);
void I2C5_ER_IRQHandler(void);
void FDCAN3_IT0_IRQHandler(void);
void FDCAN3_IT1_IRQHandler(void);
void TIM23_IRQHandler(void);
void TIM24_IRQHandler(void);
