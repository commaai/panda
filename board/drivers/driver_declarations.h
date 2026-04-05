#ifndef DRIVER_DECLARATIONS_H
#define DRIVER_DECLARATIONS_H

#include <stdint.h>
#include <stdbool.h>

// Include board/can.h to get CANPacket_t and can_ring definitions correctly
#include "board/can.h"

typedef struct board board;
typedef struct harness_configuration harness_configuration;

typedef struct {
  ADC_TypeDef *adc;
  uint8_t channel;
  uint8_t sample_time;
  uint8_t oversampling;
} adc_signal_t;

struct harness_configuration {
  const bool has_harness;
  GPIO_TypeDef * const GPIO_SBU1;
  GPIO_TypeDef * const GPIO_SBU2;
  GPIO_TypeDef * const GPIO_relay_SBU1;
  GPIO_TypeDef * const GPIO_relay_SBU2;
  const uint8_t pin_SBU1;
  const uint8_t pin_SBU2;
  const uint8_t pin_relay_SBU1;
  const uint8_t pin_relay_SBU2;
  const uint8_t adc_channel_SBU1;
  const uint8_t adc_channel_SBU2;
  const adc_signal_t adc_signal_SBU1;
  const adc_signal_t adc_signal_SBU2;
};

// ******************** constants ********************
#define PANDA_CAN_CNT 3U
#define REGISTER_MAP_SIZE 0x3FFU
#define HASHING_PRIME 23U
#define FIFO_SIZE_INT 0x400U
#define SPI_BUF_SIZE 4096U
#define NUM_INTERRUPTS 163U

// ******************** can structures ********************
typedef struct __attribute__((packed)) {
  uint8_t bus_off;
  uint32_t bus_off_cnt;
  uint8_t error_warning;
  uint8_t error_passive;
  uint8_t last_error; // real time LEC value
  uint8_t last_stored_error; // last LEC positive error code stored
  uint8_t last_data_error; // DLEC (for CANFD only)
  uint8_t last_data_stored_error; // last DLEC positive error code stored (for CANFD only)
  uint8_t receive_error_cnt; // Actual state of the receive error counter, values between 0 and 127. FDCAN_ECR.REC
  uint8_t transmit_error_cnt; // Actual state of the transmit error counter, values between 0 and 255. FDCAN_ECR.TEC
  uint32_t total_error_cnt; // How many times any error interrupt was invoked
  uint32_t total_tx_lost_cnt; // Tx event FIFO element lost
  uint32_t total_rx_lost_cnt; // Rx FIFO 0 message lost due to FIFO full condition
  uint32_t total_tx_cnt;
  uint32_t total_rx_cnt;
  uint32_t total_fwd_cnt; // Messages forwarded from one bus to another
  uint32_t total_tx_checksum_error_cnt;
  uint16_t can_speed;
  uint16_t can_data_speed;
  uint8_t canfd_enabled;
  uint8_t brs_enabled;
  uint8_t canfd_non_iso;
  uint32_t irq0_call_rate;
  uint32_t irq1_call_rate;
  uint32_t irq2_call_rate;
  uint32_t can_core_reset_cnt;
} can_health_t;

typedef struct {
  uint8_t bus_lookup;
  uint8_t can_num_lookup;
  int8_t forwarding_bus;
  uint32_t can_speed;
  uint32_t can_data_speed;
  bool canfd_auto;
  bool canfd_enabled;
  bool brs_enabled;
  bool canfd_non_iso;
} bus_config_t;

// ******************** uart ********************
typedef struct uart_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  uint8_t *elems_tx;
  uint32_t tx_fifo_size;
  volatile uint16_t w_ptr_rx;
  volatile uint16_t r_ptr_rx;
  uint8_t *elems_rx;
  uint32_t rx_fifo_size;
  void *uart;
  void (*callback)(struct uart_ring*);
  bool overwrite;
} uart_ring;

// ******************** interrupts ********************
typedef struct {
  void (*handler)(void);
  uint32_t call_counter;
  uint32_t max_call_rate;
  uint32_t call_rate;
  uint32_t call_rate_fault;
} interrupt;

#define REGISTER_INTERRUPT(irq_num, irq_handler, call_rate_max, rate_fault) \
  interrupts[irq_num].handler = (irq_handler); \
  interrupts[irq_num].max_call_rate = (call_rate_max); \
  interrupts[irq_num].call_rate_fault = (rate_fault);

extern interrupt interrupts[NUM_INTERRUPTS];

#endif
