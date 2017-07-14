#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"

#include "driver/uart.h"

#define ELM_PORT 35000

static struct espconn elm_conn;
static esp_tcp elm_proto;

//static const int pin = 2;

static char stripped_msg[0x100];
static uint16 stripped_msg_len = 0;

static char in_msg[0x100];
static uint16 in_msg_len = 0;

static char rsp_buff[0x200];
static uint16 rsp_buff_len = 0;

static bool elm_mode_echo = true;

static uint16_t ICACHE_FLASH_ATTR elm_strip(char *data, uint16_t lenin,
                                            char *outbuff, uint16_t outbufflen) {
  uint16_t count = 0;
  uint16_t i;

  for(i = 0; i < lenin; i++) {
    if(count >= outbufflen) break;
    if(data[i] == ' ') continue;
    if(data[i] >= 'a' && data[i] <= 'z'){
      outbuff[count++] = data[i] - ('a' - 'A');
    } else {
      outbuff[count++] = data[i];
    }
    if(data[i] == '\r') break;
  }
  return count;
}

static int ICACHE_FLASH_ATTR elm_msg_find_cr_or_eos(char *data, uint16_t len){
  uint16_t i;
  for(i = 0; i < len; i++)
    if(data[i] == '\r') {
      i++;
      break;
    }
  return i;
}

static int ICACHE_FLASH_ATTR elm_msg_is_at_cmd(char *data, uint16_t len){
  if(len < 4) return 0;
  if(data[0] == 'A' && data[1] == 'T') return 1;
  return 0;
}

static void ICACHE_FLASH_ATTR elm_append_rsp(char *data, uint16_t len) {
  if(rsp_buff_len + len > sizeof(rsp_buff))
    len = sizeof(rsp_buff) - rsp_buff_len;
  memcpy(rsp_buff + rsp_buff_len, data, len);
  rsp_buff_len += len;
}

static void ICACHE_FLASH_ATTR elm_append_in_msg(char *data, uint16_t len) {
  if(in_msg_len + len > sizeof(in_msg))
    len = sizeof(in_msg) - in_msg_len;
  memcpy(in_msg + in_msg_len, data, len);
  in_msg_len += len;
}

static void ICACHE_FLASH_ATTR elm_process_at_cmd(char *cmd, uint16_t len) {
  char cmdchar = cmd[0];
  switch(cmdchar){
  case 'E':
    if(cmd[1] == '0'){
      elm_mode_echo = false;
      elm_append_rsp("OK\r\r", 4);
    } else if(cmd[1] == '1'){
      elm_mode_echo = true;
      elm_append_rsp("OK\r\r", 4);
    } else {
      elm_append_rsp("?\r\r", 3);
    }
    break;
  case 'Z':
    espconn_send(&elm_conn, "\r\rELM327 v2.1\r\n>", 16);
    break;
  default:
    elm_append_rsp("?\r\r", 3);
  }
}

static void ICACHE_FLASH_ATTR elm_rx_cb(void *arg, char *data, uint16_t len) {
  // Unclear if multiple ELM instructions in same packet should be supported.
  // Limit one command per packet for now.
  rsp_buff_len = 0;
  len = elm_msg_find_cr_or_eos(data, len);

  if(!(len == 1 && data[0] == '\r') && in_msg_len && in_msg[in_msg_len-1] == '\r'){
    in_msg_len = 0;
  }

  if(!(len == 1 && data[0] == '\r' && in_msg_len && in_msg[in_msg_len-1] == '\r')) {
    // Not Repeating last message
    elm_append_in_msg(data, len); //Aim to remove this memcpy
  }
  if(elm_mode_echo)
    elm_append_rsp(in_msg, in_msg_len); // ECHO BACK

  if(in_msg_len > 0 && in_msg[in_msg_len-1] == '\r') { //Got a full line
    stripped_msg_len = elm_strip(in_msg, in_msg_len, stripped_msg, sizeof(stripped_msg));

    if(elm_msg_is_at_cmd(stripped_msg, stripped_msg_len)) {
      elm_process_at_cmd(stripped_msg+2, stripped_msg_len-2);
    }
    elm_append_rsp(">", 1);
    //in_msg_len = 0;
  }

  espconn_send(&elm_conn, rsp_buff, rsp_buff_len);

  //Just clear the buffer if full with no termination
  if(in_msg_len == sizeof(in_msg) && in_msg[in_msg_len-1] != '\r')
    in_msg_len = 0;

  //uart0_tx_buffer(data, len);
}

void ICACHE_FLASH_ATTR elm_tcp_connect_cb(void *arg) {
  struct espconn *conn = (struct espconn *)arg;
  espconn_set_opt(&elm_conn, ESPCONN_NODELAY);
  espconn_regist_recvcb(conn, elm_rx_cb);
  espconn_send(&elm_conn, "\r\rELM327 v2.1\r\r>", 16);
}

void ICACHE_FLASH_ATTR elm327_init() {
  // control listener
  elm_proto.local_port = ELM_PORT;
  elm_conn.type = ESPCONN_TCP;
  elm_conn.state = ESPCONN_NONE;
  elm_conn.proto.tcp = &elm_proto;
  espconn_regist_connectcb(&elm_conn, elm_tcp_connect_cb);
  espconn_accept(&elm_conn);
  espconn_regist_time(&elm_conn, 60, 0); // 60s timeout for all connections
}
