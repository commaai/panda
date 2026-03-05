#pragma once

#include "board/can.h"
#include "board/health.h"
#include "board/crc.h"
#ifdef STM32H7
#include "board/stm32h7/lladc_declarations.h"
#endif

// ******************** bootkick ********************

extern bool bootkick_reset_triggered;

void bootkick_tick(bool ignition, bool recent_heartbeat);

// ******************** can_common ********************
// Type definitions and function declarations moved to drivers/can_common.h

// ******************** clock_source ********************
// Function declarations moved to drivers/clock_source.h

// ******************** fan ********************

struct fan_state_t {
  uint16_t tach_counter;
  uint16_t rpm;
  uint8_t power;
  float error_integral;
  uint8_t cooldown_counter;
};
extern struct fan_state_t fan_state;

void fan_set_power(uint8_t percentage);
void llfan_init(void);
void fan_init(void);
// Call this at FAN_TICK_FREQ
void fan_tick(void);

// ******************** fdcan ********************
#ifdef STM32H7

typedef struct {
  volatile uint32_t header[2];
  volatile uint32_t data_word[CANPACKET_DATA_SIZE_MAX/4U];
} canfd_fifo;

extern FDCAN_GlobalTypeDef *cans[PANDA_CAN_CNT];

#define CAN_ACK_ERROR 3U

void can_clear_send(FDCAN_GlobalTypeDef *FDCANx, uint8_t can_number);
void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);

void process_can(uint8_t can_number);
void can_rx(uint8_t can_number);
bool can_init(uint8_t can_number);

// ******************** harness ********************

#define HARNESS_STATUS_NC 0U
#define HARNESS_STATUS_NORMAL 1U
#define HARNESS_STATUS_FLIPPED 2U

struct harness_t {
  uint8_t status;
  uint16_t sbu1_voltage_mV;
  uint16_t sbu2_voltage_mV;
  bool relay_driven;
  bool sbu_adc_lock;
};
extern struct harness_t harness;

struct harness_configuration {
  GPIO_TypeDef * const GPIO_SBU1;
  GPIO_TypeDef * const GPIO_SBU2;
  GPIO_TypeDef * const GPIO_relay_SBU1;
  GPIO_TypeDef * const GPIO_relay_SBU2;
  const uint8_t pin_SBU1;
  const uint8_t pin_SBU2;
  const uint8_t pin_relay_SBU1;
  const uint8_t pin_relay_SBU2;
  const adc_signal_t adc_signal_SBU1;
  const adc_signal_t adc_signal_SBU2;
};

// The ignition relay is only used for testing purposes
void set_intercept_relay(bool intercept, bool ignition_relay);
bool harness_check_ignition(void);
void harness_tick(void);
void harness_init(void);

#endif // STM32H7

// ******************** registers ********************

// 10 bit hash with 23 as a prime
#define REGISTER_MAP_SIZE 0x3FFU
#define HASHING_PRIME 23U

// Do not put bits in the check mask that get changed by the hardware
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
// Set individual bits. Also add them to the check_mask.
// Do not use this to change bits that get reset by the hardware
void register_set_bits(volatile uint32_t *addr, uint32_t val);
// Clear individual bits. Also add them to the check_mask.
// Do not use this to clear bits that get set by the hardware
void register_clear_bits(volatile uint32_t *addr, uint32_t val);
// To be called periodically
void check_registers(void);
void init_registers(void);

// ******************** simple_watchdog ********************

typedef struct simple_watchdog_state_t {
  uint32_t fault;
  uint32_t last_ts;
  uint32_t threshold;
} simple_watchdog_state_t;

void simple_watchdog_kick(void);
void simple_watchdog_init(uint32_t fault, uint32_t threshold);

// ******************** spi ********************

// got max rate from hitting a non-existent endpoint
// in a tight loop, plus some buffer
#define SPI_IRQ_RATE  16000U

#define SPI_BUF_SIZE 4096U
extern uint8_t spi_buf_rx[SPI_BUF_SIZE];
extern uint8_t spi_buf_tx[SPI_BUF_SIZE];

extern uint16_t spi_error_count;

void can_tx_comms_resume_spi(void);
void spi_init(void);
void spi_rx_done(void);
void spi_tx_done(bool reset);

// ******************** uart ********************

#define FIFO_SIZE_INT 0x400U

typedef struct uart_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  uint8_t *elems_tx;
  uint32_t tx_fifo_size;
  volatile uint16_t w_ptr_rx;
  volatile uint16_t r_ptr_rx;
  uint8_t *elems_rx;
  uint32_t rx_fifo_size;
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
  bool overwrite;
} uart_ring;

// UART ring buffers (defined in uart.c or stm32h7_config.h for BOOTSTUB)
extern uart_ring uart_ring_debug;
extern uart_ring uart_ring_som_debug;

// Function declarations in drivers/uart.h

// ******************** usb ********************
// Function declarations moved to drivers/usb.h
