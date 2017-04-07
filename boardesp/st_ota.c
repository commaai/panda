#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"

#include "driver/gpio16.h"
#include "driver/uart.h"

#define ST_OTA_PORT 3333
#define ST_UART_PORT 3334

static struct espconn ota_conn;
static esp_tcp ota_proto;

static struct espconn uart_conn;
static esp_tcp uart_proto;

#define ST_RESET_PIN 16

static void ICACHE_FLASH_ATTR null_function(char c) {
}

// for out of band access
static void ICACHE_FLASH_ATTR ota_rx_cb(void *arg, char *data, uint16_t len) {
  int i;
  for (i = 0; i < len; i++) {
    if (data[i] == 'n') {
      // no boot mode (pull high)
      gpio_output_set((1 << 4), 0, (1 << 4), 0);
      // init the uart for normal mode
      uart0_init(0);
    } else if (data[i] == 'b') {
      // boot mode (pull low)
      gpio_output_set(0, (1 << 4), (1 << 4), 0);
      // init the uart for boot mode
      uart0_init(1);
    } else if (data[i] == 'r') {
      // reset the ST
      gpio16_output_conf();
      gpio16_output_set(0);
      os_delay_us(10000);
      gpio16_output_set(1);
    }
  }
}

static void ICACHE_FLASH_ATTR ota_tcp_connect_cb(void *arg) {
  struct espconn *conn = (struct espconn *)arg;
  os_printf("ST OTA connection received from "IPSTR":%d\n",
            IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

  espconn_regist_recvcb(conn, ota_rx_cb);
  //espconn_regist_disconcb(conn, ota_disc_cb);
  //espconn_regist_reconcb(conn, ota_recon_cb);

  char message[] = "ST control v2\r\n";
  espconn_send(&ota_conn, message, strlen(message));
}

// for UART access
static void ICACHE_FLASH_ATTR uart_rx_cb(void *arg, char *data, uint16_t len) {
  // write data to UART
  uart0_tx_buffer(data, len);
}

static void ICACHE_FLASH_ATTR uart_tcp_connect_cb(void *arg) {
  // go
  struct espconn *conn = (struct espconn *)arg;
  espconn_regist_recvcb(conn, uart_rx_cb);
}

// size is only a byte, so this is max

static void uart0_rx_intr_handler(void *para) {
  uint8 rx_buf[0x100];

  if (UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(0)) & UART_RXFIFO_TOUT_INT_ST)) {
    uint8 fifo_len = (READ_PERI_REG(UART_STATUS(UART0))>>UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
    int i = 0;
    for (i = 0; i < fifo_len; i++) {
      uint8 d_tmp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
      rx_buf[i] = d_tmp;
    }
    WRITE_PERI_REG(UART_INT_CLR(0), UART_RXFIFO_TOUT_INT_CLR);

    // echo
    //uart0_tx_buffer(rx_buf, fifo_len);

    // send to network
    espconn_send(&uart_conn, rx_buf, fifo_len);
  } else {
    // WTF
    os_printf("WTF rx %X\n", READ_PERI_REG(UART_INT_ST(0)));
  }
}

void ICACHE_FLASH_ATTR uart0_init(int flashing_mode) {
  // copy uart_config from the driver
  ETS_UART_INTR_ATTACH(uart0_rx_intr_handler, NULL);

  // make TXD be TXD
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

  // set baud rate to 115200
  uart_div_modify(0, UART_CLK_FREQ / 115200);

  if (flashing_mode) {
    // even parity for ST
    WRITE_PERI_REG(UART_CONF0(0), ((STICK_PARITY_EN & UART_PARITY_EN_M)  <<  UART_PARITY_EN_S)
                                | ((EVEN_BITS & UART_PARITY_M)  <<UART_PARITY_S )
                                | ((ONE_STOP_BIT & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                                | ((EIGHT_BITS & UART_BIT_NUM) << UART_BIT_NUM_S));
  } else {
    // no parity
    WRITE_PERI_REG(UART_CONF0(0), ((STICK_PARITY_DIS & UART_PARITY_EN_M)  <<  UART_PARITY_EN_S)
                                | ((NONE_BITS & UART_PARITY_M)  <<UART_PARITY_S )
                                | ((ONE_STOP_BIT & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                                | ((EIGHT_BITS & UART_BIT_NUM) << UART_BIT_NUM_S));
  }

  // clear rx and tx fifo
  SET_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST | UART_TXFIFO_RST);

  // set rx fifo trigger?
  WRITE_PERI_REG(UART_CONF1(0), ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
                                (0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
                                UART_RX_TOUT_EN |
                                ((0x10 & UART_TXFIFO_EMPTY_THRHD)<<UART_TXFIFO_EMPTY_THRHD_S));

  // clear all interrupt
  WRITE_PERI_REG(UART_INT_CLR(0), 0xffff);

  // enable rx_interrupt
  SET_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_TOUT_INT_ENA);
  //SET_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA);
  //SET_PERI_REG_MASK(UART_INT_ENA(0), UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_OVF_INT_ENA);

  // actually enable the UART interrupt
  ETS_UART_INTR_ENABLE();

  if (flashing_mode) {
    // disable other UART writers
    // TODO: looks like this must be after UART init
    os_install_putc1((void*)null_function);
  } else {
    UART_SetPrintPort(0);
  }
}

void ICACHE_FLASH_ATTR st_ota_init() {

  // control listener
  ota_proto.local_port = ST_OTA_PORT;
  ota_conn.type = ESPCONN_TCP;
  ota_conn.state = ESPCONN_NONE;
  ota_conn.proto.tcp = &ota_proto;
  espconn_regist_connectcb(&ota_conn, ota_tcp_connect_cb);
  espconn_accept(&ota_conn);

  // uart listener
  uart_proto.local_port = ST_UART_PORT;
  uart_conn.type = ESPCONN_TCP;
  uart_conn.state = ESPCONN_NONE;
  uart_conn.proto.tcp = &uart_proto;
  espconn_regist_connectcb(&uart_conn, uart_tcp_connect_cb);
  espconn_accept(&uart_conn);
}

