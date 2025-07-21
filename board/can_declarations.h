#pragma once

// bump this when changing the CAN packet
#define CAN_PACKET_VERSION 4

#define CANPACKET_HEAD_SIZE 6U

#if !defined(STM32F4)
  #define CANFD
  #define CANPACKET_DATA_SIZE_MAX 64U
#else
  #define CANPACKET_DATA_SIZE_MAX 8U
#endif

// Portable CAN packet structure without bit fields
// Total header size: 6 bytes (1 flags + 4 addr + 1 checksum)
typedef struct {
  uint8_t flags;        // bit layout: ERRDDDDB (E=extended, RR=returned+rejected, DDDD=dlc, B=bus:3+fd:1)
  uint32_t addr;        // 32-bit address (only 29 bits used, stored in native endianness)
  uint8_t checksum;
  uint8_t data[CANPACKET_DATA_SIZE_MAX];
} __attribute__((packed, aligned(4))) CANPacket_t;

// Bit field positions in flags byte
#define CAN_FLAGS_FD_MASK        0x01U
#define CAN_FLAGS_BUS_MASK       0x0EU
#define CAN_FLAGS_BUS_SHIFT      1U
#define CAN_FLAGS_DLC_MASK       0xF0U
#define CAN_FLAGS_DLC_SHIFT      4U

// Additional flag bits stored in separate byte positions (for compatibility)
#define CAN_FLAGS_EXTENDED_POS   7U
#define CAN_FLAGS_RETURNED_POS   6U
#define CAN_FLAGS_REJECTED_POS   5U

// Getter macros
#define GET_BUS(msg)         (((msg)->flags & CAN_FLAGS_BUS_MASK) >> CAN_FLAGS_BUS_SHIFT)
#define GET_LEN(msg)         (dlc_to_len[((msg)->flags & CAN_FLAGS_DLC_MASK) >> CAN_FLAGS_DLC_SHIFT])
#define GET_ADDR(msg)        ((msg)->addr & 0x1FFFFFFFU)  // Mask to 29 bits
#define GET_FD(msg)          ((msg)->flags & CAN_FLAGS_FD_MASK)
#define GET_EXTENDED(msg)    (((msg)->addr >> 31U) & 1U)
#define GET_RETURNED(msg)    (((msg)->addr >> 30U) & 1U)
#define GET_REJECTED(msg)    (((msg)->addr >> 29U) & 1U)
#define GET_DLC(msg)         (((msg)->flags & CAN_FLAGS_DLC_MASK) >> CAN_FLAGS_DLC_SHIFT)

// MISRA-compliant setter functions
static inline void SET_BUS(CANPacket_t *msg, uint8_t val) {
  msg->flags = (msg->flags & ~CAN_FLAGS_BUS_MASK) | (((val) << CAN_FLAGS_BUS_SHIFT) & CAN_FLAGS_BUS_MASK);
}
static inline void SET_DLC(CANPacket_t *msg, uint8_t val) {
  msg->flags = (msg->flags & ~CAN_FLAGS_DLC_MASK) | (((val) << CAN_FLAGS_DLC_SHIFT) & CAN_FLAGS_DLC_MASK);
}
static inline void SET_FD(CANPacket_t *msg, uint8_t val) {
  msg->flags = (msg->flags & ~CAN_FLAGS_FD_MASK) | ((val) & CAN_FLAGS_FD_MASK);
}
static inline void SET_ADDR(CANPacket_t *msg, uint32_t val) {
  msg->addr = (msg->addr & 0xE0000000U) | ((val) & 0x1FFFFFFFU);
}
static inline void SET_EXTENDED(CANPacket_t *msg, uint8_t val) {
  msg->addr = (msg->addr & 0x7FFFFFFFU) | (((val) & 1U) << 31U);
}
static inline void SET_RETURNED(CANPacket_t *msg, uint8_t val) {
  msg->addr = (msg->addr & 0xBFFFFFFFU) | (((val) & 1U) << 30U);
}
static inline void SET_REJECTED(CANPacket_t *msg, uint8_t val) {
  msg->addr = (msg->addr & 0xDFFFFFFFU) | (((val) & 1U) << 29U);
}
