#include "fake_stm.h"
#include "config.h"
#include "can.h"

bool can_init(uint8_t can_number) { return true; }
void process_can(uint8_t can_number) { }
//int safety_tx_hook(CANPacket_t *to_send) { return 1; }

void can_tx_comms_resume_usb(void) { };
void can_tx_comms_resume_spi(void) { };

#include "health.h"
#include "sys/faults.h"
#include "libc.h"
#include "boards/board_declarations.h"
#include "opendbc/safety/safety.h"
#include "main_definitions.h"
#include "drivers/can_common.h"

// CAN queue definitions for libpanda test build
// (in the firmware these are defined in board/drivers/can_common.c)
#define CAN_RX_BUFFER_SIZE 4096U
#define CAN_TX_BUFFER_SIZE 416U
static CANPacket_t elems_rx_q[CAN_RX_BUFFER_SIZE];
can_ring can_rx_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_RX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_rx_q };
static CANPacket_t elems_tx1_q[CAN_TX_BUFFER_SIZE];
can_ring can_tx1_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_TX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_tx1_q };
static CANPacket_t elems_tx2_q[CAN_TX_BUFFER_SIZE];
can_ring can_tx2_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_TX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_tx2_q };
static CANPacket_t elems_tx3_q[CAN_TX_BUFFER_SIZE];
can_ring can_tx3_q = { .w_ptr = 0, .r_ptr = 0, .fifo_size = CAN_TX_BUFFER_SIZE, .elems = (CANPacket_t *)&elems_tx3_q };

can_ring *rx_q = &can_rx_q;
can_ring *tx1_q = &can_tx1_q;
can_ring *tx2_q = &can_tx2_q;
can_ring *tx3_q = &can_tx3_q;

bus_config_t bus_config[PANDA_CAN_CNT] = {
  { .bus_lookup = 0U, .can_num_lookup = 0U, .forwarding_bus = -1, .can_speed = 5000U, .can_data_speed = 20000U },
  { .bus_lookup = 1U, .can_num_lookup = 1U, .forwarding_bus = -1, .can_speed = 5000U, .can_data_speed = 20000U },
  { .bus_lookup = 2U, .can_num_lookup = 2U, .forwarding_bus = -1, .can_speed = 5000U, .can_data_speed = 20000U },
};

#include "comms_definitions.h"
#include "can_comms.h"

// CAN function implementations for the test library.
// After the .h/.c split, these moved from headers to .c files
// that can't be directly included (STM32 header dependencies).
// These implementations match board/drivers/can_common.c and board/can_comms.c.

bool can_pop(can_ring *q, CANPacket_t *elem) {
  bool ret = 0;
  ENTER_CRITICAL();
  if (q->w_ptr != q->r_ptr) {
    *elem = q->elems[q->r_ptr];
    if ((q->r_ptr + 1U) == q->fifo_size) { q->r_ptr = 0; } else { q->r_ptr += 1U; }
    ret = 1;
  }
  EXIT_CRITICAL();
  return ret;
}

bool can_push(can_ring *q, const CANPacket_t *elem) {
  bool ret = false;
  uint32_t next_w_ptr;
  ENTER_CRITICAL();
  if ((q->w_ptr + 1U) == q->fifo_size) { next_w_ptr = 0; } else { next_w_ptr = q->w_ptr + 1U; }
  if (next_w_ptr != q->r_ptr) {
    q->elems[q->w_ptr] = *elem;
    q->w_ptr = next_w_ptr;
    ret = true;
  }
  EXIT_CRITICAL();
  return ret;
}

uint32_t can_slots_empty(const can_ring *q) {
  uint32_t ret = 0;
  ENTER_CRITICAL();
  if (q->w_ptr >= q->r_ptr) { ret = q->fifo_size - 1U - q->w_ptr + q->r_ptr; }
  else { ret = q->r_ptr - q->w_ptr - 1U; }
  EXIT_CRITICAL();
  return ret;
}

uint8_t calculate_checksum(const uint8_t *dat, uint32_t len) {
  uint8_t checksum = 0U;
  for (uint32_t i = 0U; i < len; i++) { checksum ^= dat[i]; }
  return checksum;
}

void can_set_checksum(CANPacket_t *packet) {
  packet->checksum = 0U;
  packet->checksum = calculate_checksum((uint8_t *)packet, CANPACKET_HEAD_SIZE + GET_LEN(packet));
}

uint32_t safety_tx_blocked = 0;
uint32_t safety_rx_invalid = 0;
uint32_t tx_buffer_overflow = 0;
uint32_t rx_buffer_overflow = 0;

bool can_tx_check_min_slots_free(uint32_t min) {
  return (can_slots_empty(&can_tx1_q) >= min) &&
         (can_slots_empty(&can_tx2_q) >= min) &&
         (can_slots_empty(&can_tx3_q) >= min);
}

can_ring *can_queues[] = {&can_tx1_q, &can_tx2_q, &can_tx3_q};

void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook) {
  if (skip_tx_hook || safety_tx_hook(to_push) != 0) {
    if (bus_number < PANDA_CAN_CNT) {
      tx_buffer_overflow += can_push(can_queues[bus_number], to_push) ? 0U : 1U;
      process_can(CAN_NUM_FROM_BUS_NUM(bus_number));
    }
  } else {
    safety_tx_blocked += 1U;
    to_push->returned = 0U;
    to_push->rejected = 1U;
    can_set_checksum(to_push);
    rx_buffer_overflow += can_push(&can_rx_q, to_push) ? 0U : 1U;
  }
}

typedef struct { uint32_t ptr; uint32_t tail_size; uint8_t data[72]; } asm_buffer;
static asm_buffer can_read_buffer = {.ptr = 0U, .tail_size = 0U};

int comms_can_read(uint8_t *data, uint32_t max_len) {
  uint32_t pos = 0U;
  if (can_read_buffer.ptr > 0U) {
    uint32_t overflow_len = MIN(max_len - pos, can_read_buffer.ptr);
    (void)memcpy(&data[pos], can_read_buffer.data, overflow_len);
    pos += overflow_len;
    (void)memcpy(can_read_buffer.data, &can_read_buffer.data[overflow_len], can_read_buffer.ptr - overflow_len);
    can_read_buffer.ptr -= overflow_len;
  }
  if (can_read_buffer.ptr == 0U) {
    CANPacket_t can_packet;
    while ((pos < max_len) && can_pop(&can_rx_q, &can_packet)) {
      uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[can_packet.data_len_code];
      if ((pos + pckt_len) <= max_len) {
        (void)memcpy(&data[pos], (uint8_t *)&can_packet, pckt_len);
        pos += pckt_len;
      } else {
        (void)memcpy(&data[pos], (uint8_t *)&can_packet, max_len - pos);
        can_read_buffer.ptr += pckt_len - (max_len - pos);
        (void)memcpy(can_read_buffer.data, &((uint8_t *)&can_packet)[(max_len - pos)], can_read_buffer.ptr);
        pos = max_len;
      }
    }
  }
  return pos;
}

static asm_buffer can_write_buffer = {.ptr = 0U, .tail_size = 0U};

void comms_can_write(const uint8_t *data, uint32_t len) {
  uint32_t pos = 0U;
  if (can_write_buffer.ptr != 0U) {
    if (can_write_buffer.tail_size <= (len - pos)) {
      CANPacket_t to_push = {0};
      (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], can_write_buffer.tail_size);
      can_write_buffer.ptr += can_write_buffer.tail_size;
      pos += can_write_buffer.tail_size;
      (void)memcpy((uint8_t *)&to_push, can_write_buffer.data, can_write_buffer.ptr);
      can_send(&to_push, to_push.bus, false);
      can_write_buffer.ptr = 0U;
      can_write_buffer.tail_size = 0U;
    } else {
      uint32_t data_size = len - pos;
      (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], data_size);
      can_write_buffer.tail_size -= data_size;
      can_write_buffer.ptr += data_size;
      pos += data_size;
    }
  }
  while (pos < len) {
    uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[(data[pos] >> 4U)];
    if ((pos + pckt_len) <= len) {
      CANPacket_t to_push = {0};
      (void)memcpy((uint8_t *)&to_push, &data[pos], pckt_len);
      can_send(&to_push, to_push.bus, false);
      pos += pckt_len;
    } else {
      (void)memcpy(can_write_buffer.data, &data[pos], len - pos);
      can_write_buffer.ptr = len - pos;
      can_write_buffer.tail_size = pckt_len - can_write_buffer.ptr;
      pos += can_write_buffer.ptr;
    }
  }
  refresh_can_tx_slots_available();
}

void comms_can_reset(void) {
  can_write_buffer.ptr = 0U;
  can_write_buffer.tail_size = 0U;
  can_read_buffer.ptr = 0U;
  can_read_buffer.tail_size = 0U;
}

void refresh_can_tx_slots_available(void) {
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_USB_BULK_TRANSFER)) {
    can_tx_comms_resume_usb();
  }
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_SPI_BULK_TRANSFER)) {
    can_tx_comms_resume_spi();
  }
}
