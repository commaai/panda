#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"

#include "driver/gpio16.h"
#include "driver/uart.h"

#define ST_OTA_PORT 3333

static struct espconn ota_conn;
static esp_tcp ota_proto;

#define ST_RESET_PIN 16

// for out of band access
static void ICACHE_FLASH_ATTR ota_rx_cb(void *arg, char *data, uint16_t len) {
  int i;
  for (i = 0; i < len; i++) {
    if (data[i] == 'n') {
      // no boot mode (pull high)
      gpio_output_set((1 << 4), 0, (1 << 4), 0);
    } else if (data[i] == 'b') {
      // boot mode (pull low)
      gpio_output_set(0, (1 << 4), (1 << 4), 0);
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

void ICACHE_FLASH_ATTR st_ota_init() {
  // control listener
  ota_proto.local_port = ST_OTA_PORT;
  ota_conn.type = ESPCONN_TCP;
  ota_conn.state = ESPCONN_NONE;
  ota_conn.proto.tcp = &ota_proto;
  espconn_regist_connectcb(&ota_conn, ota_tcp_connect_cb);
  espconn_accept(&ota_conn);
}

