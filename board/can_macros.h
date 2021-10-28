unsigned char dlc_to_len[] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};

#define GET_BUS(msg) ((msg)->bus)
#define GET_LEN(msg) (dlc_to_len[(msg)->data_len_code])
#define GET_ADDR(msg) ((msg)->addr)
#define GET_BYTE(msg, b) ((msg)->data[(b)])
#define GET_BYTES_04(msg) ((msg)->data[0] | ((msg)->data[1] << 8) | ((msg)->data[2] << 16) | ((msg)->data[3] << 24))
#define GET_BYTES_48(msg) ((msg)->data[4] | ((msg)->data[5] << 8) | ((msg)->data[6] << 16) | ((msg)->data[7] << 24))
#define GET_FLAG(value, mask) (((__typeof__(mask))(value) & (mask)) == (mask))

// Flasher and pedal with raw mailbox access
#define GET_MAILBOX_BYTE(msg, b) (((int)(b) > 3) ? (((msg)->RDHR >> (8U * ((unsigned int)(b) % 4U))) & 0xFFU) : (((msg)->RDLR >> (8U * (unsigned int)(b))) & 0xFFU))
#define GET_MAILBOX_BYTES_04(msg) ((msg)->RDLR)
#define GET_MAILBOX_BYTES_48(msg) ((msg)->RDHR)

#define CAN_INIT_TIMEOUT_MS 500U

#define CANPACKET_HEAD_SIZE 5U
