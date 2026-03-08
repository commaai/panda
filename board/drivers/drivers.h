#pragma once

#include "board/drivers/boot_state.h"
#include "board/can.h"
#include "board/health.h"
#include "board/crc.h"
#include "board/drivers/can_common.h"
#ifdef STM32H7
#include "board/stm32h7/lladc_declarations.h"
#endif

// ******************** bootkick ********************
// bootkick is only available on regular panda, not on BODY or JUNGLE variants
#if !defined(PANDA_BODY) && !defined(PANDA_JUNGLE)

extern bool bootkick_reset_triggered;

void bootkick_tick(bool ignition, bool recent_heartbeat);

#endif // !PANDA_BODY && !PANDA_JUNGLE

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

// ******************** interrupts ********************

typedef struct interrupt {
  IRQn_Type irq_type;
  void (*handler)(void);
  uint32_t call_counter;
  uint32_t call_rate;
  uint32_t max_call_rate;   // Call rate is defined as the amount of calls each second
  uint32_t call_rate_fault;
} interrupt;

void interrupt_timer_init(void);
uint32_t microsecond_timer_get(void);
void unused_interrupt_handler(void);

extern interrupt interrupts[NUM_INTERRUPTS];

#define REGISTER_INTERRUPT(irq_num, func_ptr, call_rate_max, rate_fault) \
  interrupts[irq_num].irq_type = (irq_num); \
  interrupts[irq_num].handler = (func_ptr);  \
  interrupts[irq_num].call_counter = 0U;   \
  interrupts[irq_num].call_rate = 0U;   \
  interrupts[irq_num].max_call_rate = (call_rate_max); \
  interrupts[irq_num].call_rate_fault = (rate_fault);

extern float interrupt_load;

void handle_interrupt(IRQn_Type irq_type);
// Every second
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);

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
#ifdef STM32H7

// ***************************** Definitions *****************************
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

// ***************************** Function prototypes *****************************
void debug_ring_callback(uart_ring *ring);
void uart_tx_ring(uart_ring *q);
uart_ring *get_ring_by_number(int a);
// ************************* Low-level buffer functions *************************
bool get_char(uart_ring *q, char *elem);
bool injectc(uart_ring *q, char elem);
bool put_char(uart_ring *q, char elem);
void clear_uart_buff(uart_ring *q);
// ************************ High-level debug functions **********************
void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);
#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
static void puth4(unsigned int i);
#endif
#if defined(DEBUG_SPI) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
static void hexdump(const void *a, int l);
#endif

#endif // STM32H7

// ******************** usb ********************

void usb_init(void);
void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void);
