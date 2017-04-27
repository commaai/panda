#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"

char staticpage[] = "HTTP/1.0 200 OK\nContent-Type: text/html\n\n"
"<html><body><tt>This is your comma.ai panda<br/><br/>\n"
"It's open source. Find the code <a href=\"https://github.com/commaai/panda\">here</a>";

static struct espconn web_conn;
static esp_tcp web_proto;

static void ICACHE_FLASH_ATTR web_rx_cb(void *arg, char *data, uint16_t len) {
  struct espconn *conn = (struct espconn *)arg;
  espconn_send(&web_conn, staticpage, strlen(staticpage));
  espconn_disconnect(conn);
}

void ICACHE_FLASH_ATTR web_tcp_connect_cb(void *arg) {
  struct espconn *conn = (struct espconn *)arg;
  espconn_set_opt(&web_conn, ESPCONN_NODELAY);
  espconn_regist_recvcb(conn, web_rx_cb);
}

void ICACHE_FLASH_ATTR web_init() {
  web_proto.local_port = 80;
  web_conn.type = ESPCONN_TCP;
  web_conn.state = ESPCONN_NONE;
  web_conn.proto.tcp = &web_proto;
  espconn_regist_connectcb(&web_conn, web_tcp_connect_cb);
  espconn_accept(&web_conn);
}

