#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#include "driver/uart.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

#define ELM_PORT 35000

//Version 1.5 is an invalid version used by many pirate clones
//that only partially support 1.0.
#define IDENT_MSG "ELM327 v1.0\r\r"
#define DEVICE_DESC "Panda\n\n"

#define SHOW_CONNECTION(msg, p_conn) os_printf("%s %p, proto %p, %d.%d.%d.%d:%d disconnect\r\n", \
        msg, p_conn, (p_conn)->proto.tcp, (p_conn)->proto.tcp->remote_ip[0], \
        (p_conn)->proto.tcp->remote_ip[1], (p_conn)->proto.tcp->remote_ip[2], \
        (p_conn)->proto.tcp->remote_ip[3], (p_conn)->proto.tcp->remote_port)

typedef struct _elm_tcp_conn {
  struct espconn *conn;
  struct _elm_tcp_conn *next;
} elm_tcp_conn_t;

typedef __attribute__((packed)) struct {
  uint8_t len;
  uint8_t dat[7]; //mode and data
} elm_obd_msg;

static struct espconn elm_conn;
static esp_tcp elm_proto;

static elm_tcp_conn_t *connection_list = NULL;

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
static bool elm_mode_print_spaces = true;
static uint8_t elm_mode_adaptive_timing = 1;
static bool elm_mode_allow_long = false;


static char hex_lookup[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static char* elm_protocols[] = {
  "AUTO",
  "SAE J1850 PWM",            //Pin 1 & 10, 41.6 kbit/s. Supported by Panda?
  "SAE J1850 VPW",            //Pin 1, GM vehicles. GMLAN? But 10.4 kbit/s
  "ISO 9141-2",               //KLINE (Lline optional)
  "ISO 14230-4 (KWP 5BAUD)",  //KLINE (Lline optional)
  "ISO 14230-4 (KWP FAST)",   //KLINE (Lline optional)
  "ISO 15765-4 (CAN 11/500)", //CAN
  "ISO 15765-4 (CAN 29/500)", //CAN
  "ISO 15765-4 (CAN 11/250)", //CAN
  "ISO 15765-4 (CAN 29/250)", //CAN
  "SAE J1939 (CAN 29/250)",   //CAN
  "USER1 (CAN 11/125)",       //CAN
  "USER2 (CAN 11/50)",        //CAN
};

#define ELM_PROTOCOL_COUNT (sizeof(elm_protocols)/sizeof(char*))

int ICACHE_FLASH_ATTR spi_comm(char *dat, int len, uint32_t *recvData, int recvDataLen);

static uint8_t sendData[0x14] = {0};
static uint32_t recvData[0x40] = {0};
static int ICACHE_FLASH_ATTR panda_usbemu_ctrl_write(uint8_t request_type, uint8_t request,
                                                      uint16_t value, uint16_t index, uint16_t length) {
  //self.sock.send(struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length));
  *(uint16_t*)(sendData) = 0;
  *(uint16_t*)(sendData+2) = 0;
  sendData[4] = request_type;
  sendData[5] = request;
  *(uint16_t*)(sendData+6) = value;
  *(uint16_t*)(sendData+8) = index;
  *(uint16_t*)(sendData+10) = length;

  int returned_count = spi_comm(sendData, 0x10, recvData, 0x40);
  os_printf("Got %d bytes from Panda\n", returned_count);
  return returned_count;
}

#define panda_set_can0_kbaud(kbps) panda_usbemu_ctrl_write(0x40, 0xde, 0, kbps*10, 0)
#define panda_set_safety_mode(mode) panda_usbemu_ctrl_write(0x40, 0xdc, mode, 0, 0)

#define PANDA_CAN_FLAG_TRANSMIT 1
#define PANDA_CAN_FLAG_EXTENDED 4

#define PANDA_USB_CAN_WRITE_BUS_NUM 3

static int ICACHE_FLASH_ATTR panda_usbemu_can_write(bool ext, uint32_t addr,
                                                    char *candata, uint8_t canlen) {
  //self.sock.send(struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length));
  uint32_t rir;

  if(canlen > 8) return 0;

  if(ext || addr >= 0x800){
    rir = (addr << 3) | PANDA_CAN_FLAG_TRANSMIT | PANDA_CAN_FLAG_EXTENDED;
  }else{
    rir = (addr << 21) | PANDA_CAN_FLAG_TRANSMIT;
  }

  //Wifi USB Wrapper
  *(uint16_t*)(sendData) = 3; //USB Bulk Endpoint ID.
  *(uint16_t*)(sendData+2) = canlen;
  //BULK MESSAGE
  *(uint32_t*)(sendData+4) = rir;
  *(uint32_t*)(sendData+8) = canlen | (0 << 4); //0 is CAN bus number.
  //CAN DATA
  memcpy(sendData+12, candata, canlen);
  memcpy(sendData+12+canlen, 0, 8-canlen); //Zero the rest

  int returned_count = spi_comm(sendData, 0x14, recvData, 0x40);
  os_printf("Got %d bytes from Panda\n", returned_count);
  return returned_count;
}

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
  AT_AT0, AT_AT1, AT_AT2, // Added ELM 1.2, expected by Torque
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
  AT_S0, AT_S1, // Added ELM 1.3, expected by Torque
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
  {"AL",  2, 2, AT_AL},
  {"AT0", 3, 3, AT_AT0}, // Added ELM 1.2, expected by Torque
  {"AT1", 3, 3, AT_AT1}, // Added ELM 1.2, expected by Torque
  {"AT2", 3, 3, AT_AT2}, // Added ELM 1.2, expected by Torque
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
  {"NL",  2, 2, AT_NL},
  {"S0",  2, 2, AT_S0}, // Added ELM 1.3, expected by Torque
  {"S1",  2, 2, AT_S1}, // Added ELM 1.3, expected by Torque
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
  int i;
  os_printf("DOING STUFF ");
  for(i = 0; i < len; i++)
    os_printf("%c", cmd[i]);
  os_printf("\r\n");

  switch(elm_parse_at_cmd(cmd, len)){
  case AT_AMP1: //RETURN DEVICE DESCRIPTION
    elm_append_rsp(DEVICE_DESC, sizeof(DEVICE_DESC));
    return;
  case AT_AL: //DISABLE LONG MESSAGE SUPPORT (>7 BYTES)
    elm_mode_allow_long = true;
    break;
  case AT_AT0: //DISABLE ADAPTIVE TIMING
    elm_mode_adaptive_timing = 0;
    break;
  case AT_AT1: //SET ADAPTIVE TIMING TO AUTO1
    elm_mode_adaptive_timing = 1;
    break;
  case AT_AT2: //SET ADAPTIVE TIMING TO AUTO2
    elm_mode_adaptive_timing = 2;
    break;
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
  case AT_NL: //DISABLE LONG MESSAGE SUPPORT (>7 BYTES)
    elm_mode_allow_long = false;
    break;
  case AT_S0: //DISABLE PRINTING SPACES IN ECU RESPONSES
    elm_mode_print_spaces = false;
    break;
  case AT_S1: //ENABLE PRINTING SPACES IN ECU RESPONSES
    elm_mode_print_spaces = true;
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
    elm_mode_print_spaces = true;
    elm_mode_adaptive_timing = 1;
    elm_mode_allow_long = false;
    elm_append_rsp("\r\r", 2);
    elm_append_rsp(IDENT_MSG, sizeof(IDENT_MSG)-1);
    panda_set_can0_kbaud(500);
    panda_set_safety_mode(0x1337);
    break;
  default:
    elm_append_rsp("?\r\r", 3);
    return;
  }

  elm_append_rsp("OK\r\r", 4);
}

static uint8_t ICACHE_FLASH_ATTR elm_decode_hex_byte(char* data) {
  return (elm_decode_hex_char(data[0]) << 4) | elm_decode_hex_char(data[1]);
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd(char *cmd, uint16_t len) {
  elm_obd_msg msg = {};
  msg.len = (len-1)/2;

  if((msg.len > 7 && !elm_mode_allow_long) || msg.len > 8){
    elm_append_rsp("?\r\r", 3);
    return;
  }

  msg.len = min(msg.len, 7);

  for(int i = 0; i < msg.len; i++){
    msg.dat[i] = elm_decode_hex_byte(&cmd[i*2]);
  }

  os_printf("Got data: %d bytes.\r\n  ", msg.len);
  for(int i = 0; i < 7; i++){
    os_printf("%02x ", msg.dat[i]);
  }
  os_printf("\n");

  panda_usbemu_can_write(0, 0x7DF, (uint8_t*)&msg, msg.len+1);

  elm_append_rsp("SEARCHING...\rUNABLE TO CONNECT\r\r", 32);
}

static void ICACHE_FLASH_ATTR elm_rx_cb(void *arg, char *data, uint16_t len) {
  os_printf("Got ELM Data In: '%s'\n", data);

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
      elm_process_obd_cmd(stripped_msg, stripped_msg_len);
    }
    elm_append_rsp(">", 1);
  }

  // All ELM operations are global, so send data out to all connections
  for(elm_tcp_conn_t *iter = connection_list; iter != NULL; iter = iter->next)
    espconn_send(iter->conn, rsp_buff, rsp_buff_len);

  //Just clear the buffer if full with no termination
  if(in_msg_len == sizeof(in_msg) && in_msg[in_msg_len-1] != '\r')
    in_msg_len = 0;
}

void ICACHE_FLASH_ATTR elm_tcp_disconnect_cb(void *arg){
  struct espconn *pesp_conn = (struct espconn *)arg;

  elm_tcp_conn_t * prev = NULL;
  for(elm_tcp_conn_t *iter = connection_list; iter != NULL; iter=iter->next){
    struct espconn *conn = iter->conn;
    //SHOW_CONNECTION("Considering Disconnecting", conn);
    if(!memcmp(pesp_conn->proto.tcp->remote_ip, conn->proto.tcp->remote_ip, 4) &&
       pesp_conn->proto.tcp->remote_port == conn->proto.tcp->remote_port){
      os_printf("Deleting ELM Connection!\n");
      if(prev){
        prev->next = iter->next;
      } else {
        connection_list = iter->next;
      }
      os_free(iter);
      break;
    }

    prev = iter;
  }
}

void ICACHE_FLASH_ATTR elm_tcp_connect_cb(void *arg) {
  struct espconn *pesp_conn = (struct espconn *)arg;
  //SHOW_CONNECTION("New connection", pesp_conn);
  espconn_set_opt(&elm_conn, ESPCONN_NODELAY);
  espconn_regist_recvcb(pesp_conn, elm_rx_cb);

  elm_tcp_conn_t *newconn = os_malloc(sizeof(elm_tcp_conn_t));
  if(!newconn) {
    os_printf("Failed to allocate place for connection\n");
  } else {
    newconn->next = connection_list;
    newconn->conn = pesp_conn;
    connection_list = newconn;
  }
}

void ICACHE_FLASH_ATTR elm327_init() {
  // control listener
  elm_proto.local_port = ELM_PORT;
  elm_conn.type = ESPCONN_TCP;
  elm_conn.state = ESPCONN_NONE;
  elm_conn.proto.tcp = &elm_proto;
  espconn_regist_connectcb(&elm_conn, elm_tcp_connect_cb);
  espconn_regist_disconcb(&elm_conn, elm_tcp_disconnect_cb);
  espconn_accept(&elm_conn);
  espconn_regist_time(&elm_conn, 60, 0); // 60s timeout for all connections
}
