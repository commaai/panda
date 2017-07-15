#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"

#include "driver/uart.h"

#define ELM_PORT 35000

//Version 1.5 is an invalid version used by many pirate clones
//that only partially support 1.0.
#define START_MSG "\r\rELM327 v1.5\r\r>"
#define IDENT_MSG "ELM327 v1.5\r\r"
#define DEVICE_DESC "Panda\n\n"

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
static bool elm_mode_linefeed = false;
static bool elm_mode_additional_headers = false;
static bool elm_mode_auto_protocol = true;
static uint8_t elm_selected_protocol = 1;

static char hex_lookup[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static char* elm_protocols[] = {
  "AUTO",
  "SAE J1850 PWM",
  "SAE J1850 VPW",
  "ISO 9141-2",
  "ISO 14230-4 (KWP 5BAUD)",
  "ISO 14230-4 (KWP FAST)",
  "ISO 15765-4 (CAN 11/500)",
  "ISO 15765-4 (CAN 29/500)",
  "ISO 15765-4 (CAN 11/250)",
  "ISO 15765-4 (CAN 29/250)",
  "SAE J1939 (CAN 29/250)",
  "USER1 (CAN 11/125)",
  "USER2 (CAN 11/50)",
};

#define ELM_PROTOCOL_COUNT (sizeof(elm_protocols)/sizeof(char*))

static int8_t ICACHE_FLASH_ATTR elm_decode_hex_char(char b){
  if(b >= '0' && b <= '9') return b - '0';
  if(b >= 'A' && b <= 'F') return (b - 'A') + 10;
  if(b >= 'a' && b <= 'f') return (b - 'a') + 10;
  return -1;
}

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
  if(!elm_mode_linefeed) {
    memcpy(rsp_buff + rsp_buff_len, data, len);
    rsp_buff_len += len;
  } else {
    int i;
    for(i=0; i < len && rsp_buff_len < sizeof(rsp_buff); i++){
      rsp_buff[rsp_buff_len++] = data[i];
      if(data[i] == '\r' && rsp_buff_len < sizeof(rsp_buff))
        rsp_buff[rsp_buff_len++] = '\n';
    }
  }
}

static void ICACHE_FLASH_ATTR elm_append_in_msg(char *data, uint16_t len) {
  if(in_msg_len + len > sizeof(in_msg))
    len = sizeof(in_msg) - in_msg_len;
  memcpy(in_msg + in_msg_len, data, len);
  in_msg_len += len;
}

enum at_cmd_ids_t { // FULL ELM 1.0 list
  AT_INVALID, //Fake

  AT_AMP1,
  AT_AL,
  AT_BD,
  AT_BI,
  AT_CAF0, AT_CAF1,
  AT_CF_8, AT_CF_3,
  AT_CFC0, AT_CFC1,
  AT_CM_8, AT_CM_3,
  AT_CP,
  AT_CS,
  AT_CV,
  AT_D,
  AT_DP, AT_DPN,
  AT_E0, AT_E1,
  AT_H0, AT_H1,
  AT_I,
  AT_IB10,
  AT_IB96,
  AT_L0, AT_L1,
  AT_M0, AT_M1, AT_MA,
  AT_MR,
  AT_MT,
  AT_NL,
  AT_PC,
  AT_R0, AT_R1,
  AT_RV,
  AT_SH_6, AT_SH_3,
  AT_SPA, AT_SP,
  AT_ST,
  AT_SW,
  AT_TPA, AT_TP,
  AT_WM_XYZA, AT_WM_XYZAB, AT_WM_XYZABC,
  AT_WS,
  AT_Z,
};

typedef struct {
  char* name;
  uint8_t name_len;
  uint8_t cmd_len;
  enum at_cmd_ids_t id;
} at_cmd_reg_t;

static const at_cmd_reg_t at_cmd_reg[] = {
  {"@1",  2, 2, AT_AMP1},
  {"DP",  2, 2, AT_DP},
  {"DPN", 3, 3, AT_DPN},
  {"E0",  2, 2, AT_E0},
  {"E1",  2, 2, AT_E1},
  {"H0",  2, 2, AT_H0},
  {"H1",  2, 2, AT_H1},
  {"I",   1, 1, AT_I},
  {"L0",  2, 2, AT_L0},
  {"L1",  2, 2, AT_L1},
  {"M0",  2, 2, AT_M0},
  //{"M1",  2, 2, AT_M1},
  {"SP",  2, 3, AT_SP},
  {"SPA", 3, 4, AT_SPA},
  {"Z",   1, 1, AT_Z},
};
#define AT_CMD_REG_LEN (sizeof(at_cmd_reg)/sizeof(at_cmd_reg_t))

static enum at_cmd_ids_t ICACHE_FLASH_ATTR elm_parse_at_cmd(char *cmd, uint16_t len){
  int i;
  for(i=0; i<AT_CMD_REG_LEN; i++){
    at_cmd_reg_t candidate = at_cmd_reg[i];
    if(candidate.cmd_len == len-1 && !memcmp(cmd, candidate.name, candidate.name_len))
      return candidate.id;
  }
  return AT_INVALID;
}

static void ICACHE_FLASH_ATTR elm_process_at_cmd(char *cmd, uint16_t len) {
  uint8_t tmp;
  switch(elm_parse_at_cmd(cmd, len)){
  case AT_AMP1:
    elm_append_rsp(DEVICE_DESC, sizeof(DEVICE_DESC));
    return;
  case AT_DP: //DESCRIBE THE PROTOCOL BY NAME
    if(elm_mode_auto_protocol && elm_selected_protocol != 0)
      elm_append_rsp("AUTO, ", 6);
    elm_append_rsp(elm_protocols[elm_selected_protocol],
                   strlen(elm_protocols[elm_selected_protocol]));
    elm_append_rsp("\r\r", 2);
    return;
  case AT_DPN: //DESCRIBE THE PROTOCOL BY NUMBER
    //TODO: Required. Report currently selected protocol
    if(elm_mode_auto_protocol)
      elm_append_rsp("A", 1);
    elm_append_rsp(&hex_lookup[elm_selected_protocol], 1);
    elm_append_rsp("\r\r", 2);
    return; // Don't display 'OK'
  case AT_E0: //ECHO OFF
    elm_mode_echo = false;
    break;
  case AT_E1: //ECHO ON
    elm_mode_echo = true;
    break;
  case AT_H0: //SHOW FULL CAN HEADERS OFF
    elm_mode_additional_headers = false;
    break;
  case AT_H1: //SHOW FULL CAN HEADERS ON
    elm_mode_additional_headers = true;
    break;
  case AT_I: //IDENTIFY SELF
    elm_append_rsp(IDENT_MSG, sizeof(IDENT_MSG)-1);
    return;
  case AT_L0: //LINEFEED OFF
    elm_mode_linefeed = false;
  case AT_L1: //LINEFEED ON
    elm_mode_linefeed = true;
    break;
  case AT_M0: //DISABLE NONVOLATILE STORAGE
    //Memory storage is likely unnecessary
    break;
  case AT_SP: //SET PROTOCOL
    tmp = elm_decode_hex_char(cmd[2]);
    if(tmp == -1 || tmp >= ELM_PROTOCOL_COUNT) {
      elm_append_rsp("?\r\r", 3);
      return;
    }
    elm_selected_protocol = tmp;
    elm_mode_auto_protocol = (tmp == 0);
    break;
  case AT_SPA: //SET PROTOCOL WITH AUTO FALLBACK
    tmp = elm_decode_hex_char(cmd[3]);
    if(tmp == -1 || tmp >= ELM_PROTOCOL_COUNT) {
      elm_append_rsp("?\r\r", 3);
      return;
    }
    elm_selected_protocol = tmp;
    elm_mode_auto_protocol = true;
    break;
  case AT_Z: //RESET
    elm_mode_echo = true;
    elm_mode_linefeed = false;
    elm_mode_additional_headers = false;
    elm_mode_auto_protocol = true;
    elm_selected_protocol = 1;
    elm_append_rsp(START_MSG, sizeof(START_MSG)-1);
    break;
  default:
    elm_append_rsp("?\r\r", 3);
    return;
  }

  elm_append_rsp("OK\r\r", 4);
}

static void ICACHE_FLASH_ATTR elm_rx_cb(void *arg, char *data, uint16_t len) {
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
    elm_append_rsp(in_msg, in_msg_len);

  if(in_msg_len > 0 && in_msg[in_msg_len-1] == '\r') { //Got a full line
    stripped_msg_len = elm_strip(in_msg, in_msg_len, stripped_msg, sizeof(stripped_msg));

    if(elm_msg_is_at_cmd(stripped_msg, stripped_msg_len)) {
      elm_process_at_cmd(stripped_msg+2, stripped_msg_len-2);
    } else {
      //TODO: Implement CAN writes
      elm_append_rsp("SEARCHING...\rUNABLE TO CONNECT\r\r", 32);
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
  espconn_send(&elm_conn, START_MSG, sizeof(START_MSG)-1);
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
