#ifndef BOARD_DRIVERS_DRIVERS_H
#define BOARD_DRIVERS_DRIVERS_H

// The firmware, the bootstub and the body are each one translation unit built
// from these header-only drivers, so the driver functions are static. The ones
// that not all three of them call are static inline, so GCC doesn't warn that
// they're unused.

#include "board/can.h"
#include "board/health.h"
#include "board/crc.h"
#ifdef STM32H7
#include "board/stm32h7/lladc_declarations.h"
#endif

// ******************** can_common ********************

typedef struct {
  // only accessed in critical sections
  uint32_t w_ptr;
  uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

typedef struct {
  uint8_t bus_lookup;
  uint8_t can_num_lookup;
  uint32_t can_speed;
  uint32_t can_data_speed;
  bool canfd_auto;
  bool canfd_enabled;
  bool brs_enabled;
  bool canfd_non_iso;
} bus_config_t;

// helpers
#define WORD_TO_BYTE_ARRAY(dst8, src32) (dst8)[0] = (uint8_t)((src32) & 0xFFU); (dst8)[1] = (uint8_t)(((src32) >> 8U) & 0xFFU); (dst8)[2] = (uint8_t)(((src32) >> 16U) & 0xFFU); (dst8)[3] = (uint8_t)(((src32) >> 24U) & 0xFFU)
#define BYTE_ARRAY_TO_WORD(dst32, src8) ((dst32) = (uint32_t)(src8)[0] | ((uint32_t)(src8)[1] << 8U) | ((uint32_t)(src8)[2] << 16U) | ((uint32_t)(src8)[3] << 24U))
static inline uint16_t read_u16_le(const uint8_t *src8) {
  return src8[0] | ((uint16_t)src8[1] << 8U);
}

// ********************* interrupt safe queue *********************
// not static, tests/libpanda calls these
bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, const CANPacket_t *elem);
uint32_t can_slots_empty(const can_ring *q);

#define CANIF_FROM_CAN_NUM(num) (cans[(num)])
#define BUS_NUM_FROM_CAN_NUM(num) (bus_config[(num)].bus_lookup)
#define CAN_NUM_FROM_BUS_NUM(num) (bus_config[(num)].can_num_lookup)

// ******************** fan ********************

struct fan_state_t {
  uint16_t tach_counter;
  uint16_t rpm;
  uint8_t power;
  uint8_t cooldown_counter;
};

// ******************** fdcan ********************
#ifdef STM32H7

typedef struct {
  volatile uint32_t header[2];
  volatile uint32_t data_word[CANPACKET_DATA_SIZE_MAX/4U];
} canfd_fifo;

#define CAN_ACK_ERROR 3U

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

// ******************** interrupts ********************

typedef struct {
  void (*handler)(void);
  uint32_t call_counter;
  uint32_t call_rate;
  uint32_t max_call_rate;   // Call rate is defined as the amount of calls each second
  uint32_t min_call_rate;
  uint32_t call_rate_fault;
} interrupt;

typedef struct {
  uint32_t last_ts;
  uint32_t prev_ts;
  uint32_t mismatches;
} timer_check_t;
static bool timer_check(timer_check_t *check, uint32_t *dt_out);
static void interrupt_timer_init(void);
uint32_t microsecond_timer_get(void);

#define REGISTER_INTERRUPT(irq_num, func_ptr, call_rate_max, rate_fault) \
  interrupts[(irq_num)].handler = (func_ptr);  \
  interrupts[(irq_num)].call_counter = 0U;   \
  interrupts[(irq_num)].call_rate = 0U;   \
  interrupts[(irq_num)].max_call_rate = (call_rate_max); \
  interrupts[(irq_num)].call_rate_fault = (rate_fault);

#endif // STM32H7

// ******************** registers ********************

// 10 bit hash with 23 as a prime
#define REGISTER_MAP_SIZE 0x400U
#define HASHING_PRIME 23U

// ******************** simple_watchdog ********************

typedef struct {
  uint32_t fault;
  uint32_t last_ts;
  uint32_t threshold;
} simple_watchdog_state_t;

// ******************** spi ********************

// got max rate from hitting a non-existent endpoint
// in a tight loop, plus some buffer
#define SPI_IRQ_RATE  16000U

#define SPI_BUF_SIZE 4096U

// ******************** debug ********************
static void print(const char *a);
static void puth(unsigned int i);

// ******************** usb ********************

static void refresh_can_tx_slots_available(void);

// ******************** comms ********************
// USB and SPI hand requests to these. The firmware, the bootstub and the body each implement them

// A control request. USB fills it from the setup packet, SPI decodes it from its endpoint 0 data
typedef struct {
  uint8_t request;
  uint16_t param1;
  uint16_t param2;
  uint16_t length;
} ControlPacket_t;

static uint32_t comms_control_handler(ControlPacket_t *req, uint8_t *resp);
static void comms_endpoint2_write(const uint8_t *data, uint32_t len);
// not static, tests/libpanda calls these
void comms_can_write(const uint8_t *data, uint32_t len);
uint32_t comms_can_read(uint8_t *data, uint32_t max_len);
void comms_can_reset(void);

#endif  // BOARD_DRIVERS_DRIVERS_H
