// When changing this struct, python/__init__.py needs to be kept up to date!
#define CAN_HEALTH_PACKET_VERSION 1
typedef struct __attribute__((packed)) {
  uint8_t bus_off;
  uint32_t bus_off_cnt;
  uint8_t error_warning;
  uint8_t error_passive;
  uint8_t last_error; // real time LEC value
  uint8_t last_stored_error; // last LEC positive error code stored
  uint8_t last_data_error; // DLEC (for CANFD only)
  uint8_t last_data_stored_error; // last DLEC positive error code stored (for CANFD only)
  uint8_t receive_error_cnt; // REC
  uint8_t transmit_error_cnt; // TEC
  uint32_t total_error_cnt; // How many times any error interrupt were invoked
  uint32_t total_tx_lost_cnt; // Tx event FIFO element Lost
  uint32_t total_rx_lost_cnt; // Rx FIFO 0 message Lost
  uint32_t total_tx_cnt;
  uint32_t total_rx_cnt;
  uint32_t total_fwd_cnt; // Messages forwarded from one bus to another
} can_health_t;
