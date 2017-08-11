#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#include "driver/uart.h"

//#define ELM_DEBUG

#define min(a,b) ((a) < (b) ? (a) : (b))
int ICACHE_FLASH_ATTR spi_comm(char *dat, int len, uint32_t *recvData, int recvDataLen);

#define ELM_PORT 35000

//Version 1.5 is an invalid version used by many pirate clones
//that only partially support 1.0.
#define IDENT_MSG "ELM327 v1.5\r\r"
#define DEVICE_DESC "Panda\n\n"

#define SHOW_CONNECTION(msg, p_conn) os_printf("%s %p, proto %p, %d.%d.%d.%d:%d disconnect\r\n", \
        msg, p_conn, (p_conn)->proto.tcp, (p_conn)->proto.tcp->remote_ip[0], \
        (p_conn)->proto.tcp->remote_ip[1], (p_conn)->proto.tcp->remote_ip[2], \
        (p_conn)->proto.tcp->remote_ip[3], (p_conn)->proto.tcp->remote_port)

const static char hex_lookup[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

typedef struct __attribute__((packed)) {
  bool tx       : 1;
  bool          : 1;
  bool ext      : 1;
  uint32_t addr : 29;

  uint8_t len   : 4;
  uint8_t bus   : 8;
  uint8_t       : 4; //unused
  uint16_t ts   : 16;
  uint8_t data[8];
} panda_can_msg_t;

//TODO: Masking is likely unnecessary for these bit fields. Check.
#define panda_get_can_addr(recv) (((recv)->ext) ? ((recv)->addr & 0x1FFFFFFF) :\
                                  (((recv)->addr >> 18) & 0x7FF))

#define PANDA_CAN_FLAG_TRANSMIT 1
#define PANDA_CAN_FLAG_EXTENDED 4

#define PANDA_USB_CAN_WRITE_BUS_NUM 3

typedef struct _elm_tcp_conn {
  struct espconn *conn;
  struct _elm_tcp_conn *next;
} elm_tcp_conn_t;

typedef struct __attribute__((packed)) {
  uint8_t len;
  uint8_t dat[7]; //mode and data
} elm_obd_msg;

static struct espconn elm_conn;
static esp_tcp elm_proto;
static elm_tcp_conn_t *connection_list = NULL;

static char stripped_msg[0x100];
static uint16 stripped_msg_len = 0;

static char in_msg[0x100];
static uint16 in_msg_len = 0;

static char rsp_buff[536]; //TCP min MTU
static uint16 rsp_buff_len = 0;

static uint8_t pandaSendData[0x14] = {0};
static uint32_t pandaRecvData[0x40] = {0};
static uint32_t pandaRecvDataDummy[0x40] = {0}; // Used for CAN write operations (no received data)

#define ELM_MODE_SELECTED_PROTOCOL_DEFAULT 6
#define ELM_MODE_TIMEOUT_DEFAULT 20;

static bool elm_mode_echo = true;
static bool elm_mode_linefeed = false;
static bool elm_mode_additional_headers = false;
static bool elm_mode_auto_protocol = true;
static uint8_t elm_selected_protocol = ELM_MODE_SELECTED_PROTOCOL_DEFAULT;
static bool elm_mode_print_spaces = true;
static uint8_t elm_mode_adaptive_timing = 1;
static bool elm_mode_allow_long = false;
static uint16_t elm_mode_timeout = ELM_MODE_TIMEOUT_DEFAULT;

/***********************************************
 ***       ELM CLI response functions        ***
 *** (for sending data back to the terminal) ***
 ***********************************************/

// All ELM operations are global, so send data out to all connections
void ICACHE_FLASH_ATTR elm_tcp_tx_flush() {
  if(!rsp_buff_len) return; // Was causing small error messages
  for(elm_tcp_conn_t *iter = connection_list; iter != NULL; iter = iter->next){
    int8_t err = espconn_send(iter->conn, rsp_buff, rsp_buff_len);
    if(err){
      os_printf("  Wifi TX error code %d\n", err);
      if(err == ESPCONN_ARG) {
        if(iter == connection_list) {
          connection_list = iter->next;
        } else {
          for(elm_tcp_conn_t *iter2 = connection_list; iter2 != NULL; iter2 = iter2->next)
            if(iter2->next == iter) {
              iter2->next = iter->next;
              break;
            }
        }
        os_printf("  deleting orphaned connection. iter: %p; conn: %p\n", iter, iter->conn);
        os_free(iter);
      }
    }
  }
  rsp_buff_len = 0;
}

static void ICACHE_FLASH_ATTR elm_append_rsp(const char *data, uint16_t len) {
  uint16_t overflow_len = 0;
  if(rsp_buff_len + len > sizeof(rsp_buff)) {
    overflow_len = rsp_buff_len + len - sizeof(rsp_buff);
    len = sizeof(rsp_buff) - rsp_buff_len;
  }
  if(!elm_mode_linefeed) {
    memcpy(rsp_buff + rsp_buff_len, data, len);
    rsp_buff_len += len;
  } else {
    for(int i=0; i < len && rsp_buff_len < sizeof(rsp_buff); i++){
      rsp_buff[rsp_buff_len++] = data[i];
      if(data[i] == '\r' && rsp_buff_len < sizeof(rsp_buff))
        rsp_buff[rsp_buff_len++] = '\n';
    }
  }
  if(overflow_len) {
    os_printf("Packet full, sending\n");
    elm_tcp_tx_flush();
    elm_append_rsp(data + len, overflow_len);
  }
}

#define elm_append_rsp_const(str) elm_append_rsp(str, sizeof(str)-1)

static void ICACHE_FLASH_ATTR elm_append_rsp_hex_byte(uint8_t num) {
  elm_append_rsp(&hex_lookup[num >> 4], 1);
  elm_append_rsp(&hex_lookup[num & 0xF], 1);
  if(elm_mode_print_spaces) elm_append_rsp_const(" ");
}

void ICACHE_FLASH_ATTR elm_append_rsp_can_msg_addr(const panda_can_msg_t *recv) {
  //Show address
  uint32_t addr = panda_get_can_addr(recv);
  if(recv->ext){
    elm_append_rsp_hex_byte(addr>>24);
    elm_append_rsp_hex_byte(addr>>16);
    elm_append_rsp_hex_byte(addr>>8);
    elm_append_rsp_hex_byte(addr);
  } else {
    elm_append_rsp(&hex_lookup[addr>>8], 1);
    elm_append_rsp_hex_byte(addr);
  }
}

/***************************************
 ***  Panda communication functions  ***
 *** (for controlling the Panda MCU) ***
 ***************************************/

static int ICACHE_FLASH_ATTR panda_usbemu_ctrl_write(uint8_t request_type, uint8_t request,
                                                     uint16_t value, uint16_t index, uint16_t length) {
  //self.sock.send(struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length));
  *(uint16_t*)(pandaSendData) = 0;
  *(uint16_t*)(pandaSendData+2) = 0;
  pandaSendData[4] = request_type;
  pandaSendData[5] = request;
  *(uint16_t*)(pandaSendData+6) = value;
  *(uint16_t*)(pandaSendData+8) = index;
  *(uint16_t*)(pandaSendData+10) = length;

  int returned_count = spi_comm(pandaSendData, 0x10, pandaRecvData, 0x40);
  if(returned_count > 0x40)
    return 0;
  return returned_count;
}

#define panda_set_can0_cbaud(cbps) panda_usbemu_ctrl_write(0x40, 0xde, 0, cbps, 0)
#define panda_set_can0_kbaud(kbps) panda_usbemu_ctrl_write(0x40, 0xde, 0, kbps*10, 0)
#define panda_set_safety_mode(mode) panda_usbemu_ctrl_write(0x40, 0xdc, mode, 0, 0)

static int ICACHE_FLASH_ATTR panda_usbemu_can_read(panda_can_msg_t** can_msgs) {
  int returned_count = spi_comm((uint8_t *)((const uint16 []){1,0}), 4, pandaRecvData, 0x40);
  if(returned_count > 0x40 || returned_count < 0){
    os_printf("CAN read got invalid length\n");
    return -1;
  }
  *can_msgs = (panda_can_msg_t*)(pandaRecvData+1);
  return returned_count/sizeof(panda_can_msg_t);
}

static int ICACHE_FLASH_ATTR panda_usbemu_can_write(bool ext, uint32_t addr,
                                                    char *candata, uint8_t canlen) {
  uint32_t rir;

  if(canlen > 8) return 0;

  if(ext || addr >= 0x800){
    rir = (addr << 3) | PANDA_CAN_FLAG_TRANSMIT | PANDA_CAN_FLAG_EXTENDED;
  }else{
    rir = (addr << 21) | PANDA_CAN_FLAG_TRANSMIT;
  }

  #define MAX_CAN_LEN 8

  //Wifi USB Wrapper
  *(uint16_t*)(pandaSendData) = PANDA_USB_CAN_WRITE_BUS_NUM; //USB Bulk Endpoint ID.
  *(uint16_t*)(pandaSendData+2) = MAX_CAN_LEN;
  //BULK MESSAGE
  *(uint32_t*)(pandaSendData+4) = rir;
  *(uint32_t*)(pandaSendData+8) = MAX_CAN_LEN | (0 << 4); //0 is CAN bus number.
  //CAN DATA
  memcpy(pandaSendData+12, candata, canlen);
  memset(pandaSendData+12+canlen, 0, MAX_CAN_LEN-canlen);
  for(int i = 12+canlen; i < 20; i++) pandaSendData[i] = 0; //Zero the rest

  /* spi_comm will erase data in the recv buffer even if you are only
   * interested in sending data that gets no response (like writing
   * can data). This behavior becomes problematic when trying to send
   * a can message while processsing received can messages. A dummy
   * recv buffer is used here so received data is not overwritten. */
  int returned_count = spi_comm(pandaSendData, 0x14, pandaRecvDataDummy, 0x40);
  if(returned_count)
    os_printf("ELM Can send expected 0 bytes back from panda. Got %d bytes instead\n", returned_count);
  if(returned_count > 0x40) return 0;
  return returned_count;
}

/****************************************
 *** String parsing utility functions ***
 ****************************************/

static int8_t ICACHE_FLASH_ATTR elm_decode_hex_char(char b){
  if(b >= '0' && b <= '9') return b - '0';
  if(b >= 'A' && b <= 'F') return (b - 'A') + 10;
  if(b >= 'a' && b <= 'f') return (b - 'a') + 10;
  return -1;
}

static uint8_t ICACHE_FLASH_ATTR elm_decode_hex_byte(char* data) {
  return (elm_decode_hex_char(data[0]) << 4) | elm_decode_hex_char(data[1]);
}

static bool ICACHE_FLASH_ATTR elm_check_valid_hex_chars(char* data, uint8_t len) {
  for(int i = 0; i < len; i++){
    char b = data[i];
    if(!((b >= '0' && b <= '9') || (b >= 'A' && b <= 'F') || (b >= 'a' && b <= 'f')))
      return 0;
  }
  return 1;
}

static uint16_t ICACHE_FLASH_ATTR elm_strip(char *data, uint16_t lenin,
                                            char *outbuff, uint16_t outbufflen) {
  uint16_t count = 0;
  for(uint16_t i = 0; i < lenin; i++) {
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

/*****************************************************
 *** ELM protocol specification and implementation ***
 *****************************************************/

typedef enum {
  AUTO, LIN, CAN11, CAN29, NA
} elm_proto_type_t;

typedef struct elm_protocol {
  bool supported;
  elm_proto_type_t type;
  uint16_t cbaud; //Centibaud (cbaud * 10 = kbaud)
  void (*process_obd)(const struct elm_protocol*, char*, uint16_t);
  void (*init)(const struct elm_protocol*);
  char* name;
} elm_protocol_t;

static const elm_protocol_t* ICACHE_FLASH_ATTR elm_current_proto();
static void ICACHE_FLASH_ATTR elm_autodetect_cb(bool);

static const elm_protocol_t elm_protocols[];
//(sizeof(elm_protocols)/sizeof(elm_protocol_t))
#define ELM_PROTOCOL_COUNT 13

#define LOOPCOUNT_FULL 4
static int loopcount = 0;
static volatile os_timer_t elm_timeout;

static bool is_auto_detecting = false;

// Used only by elm_timer_cb, so not volatile
static bool did_multimessage = false;
static bool got_msg_this_run = false;
static bool can_tx_worked = false;
static uint8_t elm_msg_mode;
static uint8_t elm_msg_pid;


void ICACHE_FLASH_ATTR elm_ISO15765_timer_cb(void *arg){
  const elm_protocol_t* proto = (const elm_protocol_t*) arg;
  loopcount--;
  if(loopcount>0) {
    for(int pass = 0; pass < 16 && loopcount; pass++){
      panda_can_msg_t *can_msgs;
      int num_can_msgs = panda_usbemu_can_read(&can_msgs);

      #ifdef ELM_DEBUG
      if(num_can_msgs) os_printf("  Received %d can messages\n", num_can_msgs);
      #endif

      if(num_can_msgs < -1) continue;
      if(!num_can_msgs) break;

      for(int i = 0; i < num_can_msgs; i++){

        panda_can_msg_t *recv = &can_msgs[i];

        #ifdef ELM_DEBUG
        os_printf("    RECV: Bus: %d; Addr: %08x; ext: %d; tx: %d; Len: %d; ",
                  recv->bus, panda_get_can_addr(recv), recv->ext, recv->tx, recv->len);
        for(int j = 0; j < recv->len; j++) os_printf("%02x ", recv->data[j]);
        os_printf("Ts: %d\n", recv->ts);
        #endif

        if (recv->bus==0 && recv->len == 8 &&
            (
             (proto->type == CAN11 && !recv->ext && (panda_get_can_addr(recv) & 0x7F8) == 0x7E8) ||
             (proto->type == CAN29 && recv->ext && (panda_get_can_addr(recv) & 0x1FFFFF00) == 0x18DAF100)
            )
           ) {
          if(recv->data[0] <= 7 &&
             recv->data[1] == (0x40|elm_msg_mode) && recv->data[2] == elm_msg_pid) {
            got_msg_this_run = true;
            loopcount = LOOPCOUNT_FULL;

            #ifdef ELM_DEBUG
            os_printf("      CAN msg response, index: %d\n", i);
            #endif

            if(!is_auto_detecting){
              if(elm_mode_additional_headers){
                elm_append_rsp_can_msg_addr(recv);
                for(int j = 0; j < recv->data[0]+1; j++) elm_append_rsp_hex_byte(recv->data[j]);
              } else {
                for(int j = 1; j < recv->data[0]+1; j++) elm_append_rsp_hex_byte(recv->data[j]);
              }

              elm_append_rsp_const("\r");
              elm_tcp_tx_flush();
            }

          } else if((recv->data[0] & 0xF0) == 0x10 &&
                    recv->data[2] == (0x40|elm_msg_mode) && recv->data[3] == elm_msg_pid) {
            got_msg_this_run = true;
            loopcount = LOOPCOUNT_FULL;
            panda_usbemu_can_write(0,
                                   (proto->type==CAN11) ?
                                   0x7E0 | (panda_get_can_addr(recv)&0x7) :
                                   (0x18DA00F1 | (((panda_get_can_addr(recv))&0xFF)<<8)),
                                   "\x30\x00\x00", 3);

            did_multimessage = true;
            os_printf("      CAN multimsg start response, index: %d, len %d\n", i,
                      ((recv->data[0]&0xF)<<8) | recv->data[1]);

            if(!is_auto_detecting){
              if(!elm_mode_additional_headers) {
                elm_append_rsp(&hex_lookup[recv->data[0]&0xF], 1);
                elm_append_rsp_hex_byte(recv->data[1]);
                elm_append_rsp_const("\r0:");
                if(elm_mode_print_spaces) elm_append_rsp_const(" ");
                for(int j = 2; j < 8; j++) elm_append_rsp_hex_byte(recv->data[j]);
              } else {
                elm_append_rsp_can_msg_addr(recv);
                for(int j = 0; j < 8; j++) elm_append_rsp_hex_byte(recv->data[j]);
              }

              elm_append_rsp_const("\r");
              elm_tcp_tx_flush();
            }

          } else if (did_multimessage && (recv->data[0] & 0xF0) == 0x20) {
            got_msg_this_run = true;
            loopcount = LOOPCOUNT_FULL;
            os_printf("      CAN multimsg data response, index: %d\n", i);

            if(!is_auto_detecting){
              if(!elm_mode_additional_headers) {
                elm_append_rsp(&hex_lookup[recv->data[0] & 0xF], 1);
                elm_append_rsp_const(":");
                if(elm_mode_print_spaces) elm_append_rsp_const(" ");
                for(int j = 1; j < 8; j++) elm_append_rsp_hex_byte(recv->data[j]);
              } else {
                elm_append_rsp_can_msg_addr(recv);
                for(int j = 0; j < 8; j++) elm_append_rsp_hex_byte(recv->data[j]);
              }
              elm_append_rsp_const("\r");
            }
          }
        } else if (recv->bus == 0x80 && recv->len == 8 &&
                   (panda_get_can_addr(recv) == ((proto->type==CAN11) ? 0x7DF : 0x18DB33F1))
                  ) {
          //Can send receipt
          #ifdef ELM_DEBUG
          os_printf("      Got CAN tx receipt\n");
          #endif
          can_tx_worked = true;
        }
      }
    }
    os_timer_arm(&elm_timeout, elm_mode_timeout, 0);
  } else {
    bool got_msg_this_run_backup = got_msg_this_run;
    if(did_multimessage) {
      os_printf("  End of multi message\n");
    } else if(!got_msg_this_run) {
      os_printf("  No data collected\n");
      if(!is_auto_detecting) {
        if(can_tx_worked) {
          elm_append_rsp_const("NO DATA\r");
        } else {
          elm_append_rsp_const("CAN ERROR\r");
        }
      }
    }
    did_multimessage = false;
    got_msg_this_run = false;
    can_tx_worked = false;

    if(!is_auto_detecting) {
      elm_append_rsp_const("\r>");
      elm_tcp_tx_flush();
    } else {
      elm_autodetect_cb(got_msg_this_run_backup);
    }
  }
}

static void ICACHE_FLASH_ATTR elm_init_ISO15765(const elm_protocol_t* proto){
  panda_set_can0_cbaud(proto->cbaud);

  // Kind of a hack to deal with Panda resending data
  // that could not be sent asap. Try to clear it away.
  // TODO: A better solution would be to clear out the
  // CAN mailboxes on the MCU when the speed changes.
  for(int pass = 0; pass < 32; pass++){
    panda_can_msg_t *can_msgs;
    int num_can_msgs = panda_usbemu_can_read(&can_msgs);
    if(num_can_msgs < -1) continue;
    //if(!num_can_msgs) break;
    for(int j=0; j<1000; j++) __asm__(""); //Small Delay
  }
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd_ISO15765(const elm_protocol_t* proto,
                                                           char *cmd, uint16_t len) {
  elm_obd_msg msg = {};
  msg.len = (len-1)/2;
  if((msg.len > 7 && !elm_mode_allow_long) || msg.len > 8) {
    elm_append_rsp_const("?\r\r>");
    return;
  }

  msg.len = min(msg.len, 7);

  for(int i = 0; i < msg.len; i++)
    msg.dat[i] = elm_decode_hex_byte(&cmd[i*2]);

  elm_msg_mode = msg.dat[0];
  elm_msg_pid = msg.dat[1];

  #ifdef ELM_DEBUG
  os_printf("Sending CAN OBD: %02x; ", msg.len);
  for(int i = 0; i < 7; i++)
    os_printf("%02x ", msg.dat[i]);
  os_printf("\n");
  #endif

  panda_usbemu_can_write(0, (proto->type==CAN11) ? 0x7DF : 0x18DB33F1,
                         (uint8_t*)&msg, msg.len+1);

  #ifdef ELM_DEBUG
  os_printf("Starting up timer\n");
  #endif

  loopcount = LOOPCOUNT_FULL;
  os_timer_disarm(&elm_timeout);
  os_timer_setfn(&elm_timeout, (os_timer_func_t *)elm_ISO15765_timer_cb, proto);
  os_timer_arm(&elm_timeout, elm_mode_timeout, 0);
}

void ICACHE_FLASH_ATTR elm_switch_proto(){
  const elm_protocol_t* proto = elm_current_proto();
  if(!proto->supported) return;
  switch(proto->type) {
  case AUTO:
    break;
  case LIN:
    break;
  case CAN11:
  case CAN29:
    break;
  default:
    break;
  }

  if(proto->init) proto->init(proto);
}

static int elm_autodetect_proto_iter;
static uint16_t elm_staged_auto_msg_len;
static char* elm_staged_auto_msg;

static void ICACHE_FLASH_ATTR elm_autodetect_cb(bool proto_worked){
  if(proto_worked) {
    os_printf("Autodetect proto success\n");
    is_auto_detecting = false;
    elm_selected_protocol = elm_autodetect_proto_iter;
    elm_current_proto()->process_obd(elm_current_proto(),
                                     elm_staged_auto_msg, elm_staged_auto_msg_len);
  } else {
    os_printf("Autodetect proto failed\n");
    for(elm_autodetect_proto_iter++; elm_autodetect_proto_iter < ELM_PROTOCOL_COUNT;
        elm_autodetect_proto_iter++){
      const elm_protocol_t *proto = &elm_protocols[elm_autodetect_proto_iter];
      if(proto->supported && proto->type != AUTO) {
      os_printf("*** AUTO trying '%s'\n", proto->name);
        proto->init(proto);
        proto->process_obd(proto, "0100", 4);
        return;
      }
    }
    is_auto_detecting = false;
    elm_append_rsp_const("UNABLE TO CONNECT\r\r>");
    elm_tcp_tx_flush();
    os_printf("Autodetect failed\n");
  }
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd_AUTO(const elm_protocol_t* proto,
                                                       char *cmd, uint16_t len) {
  elm_append_rsp_const("SEARCHING...\r");
  elm_staged_auto_msg_len = len;
  elm_staged_auto_msg = cmd;
  is_auto_detecting = true;
  for(elm_autodetect_proto_iter = 0; elm_autodetect_proto_iter < ELM_PROTOCOL_COUNT;
      elm_autodetect_proto_iter++){
    const elm_protocol_t *proto = &elm_protocols[elm_autodetect_proto_iter];
    if(proto->supported && proto->type != AUTO) {
      os_printf("*** AUTO trying '%s'\n", proto->name);
      proto->init(proto);
      proto->process_obd(proto, "0100", 4); // Try sending on the bus
      return;
    }
  }
  os_printf("ERROR: auto detect entering invalid state.\n");
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd_J1850(const elm_protocol_t* proto,
                                                        char *cmd, uint16_t len) {
  elm_append_rsp_const("NO DATA\r\r>");
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd_LIN5baud(const elm_protocol_t* proto,
                                                           char *cmd, uint16_t len) {
  elm_append_rsp_const("BUS INIT: ...ERROR\r\r>");
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd_LINFast(const elm_protocol_t* proto,
                                                          char *cmd, uint16_t len) {
  elm_append_rsp_const("BUS INIT: ERROR\r\r>");
}

static void ICACHE_FLASH_ATTR elm_process_obd_cmd_CANGen(const elm_protocol_t* proto,
                                                         char *cmd, uint16_t len) {
  elm_append_rsp_const("NO DATA\r\r>");
}

static const elm_protocol_t elm_protocols[] = {
  {true,  AUTO,  0,    elm_process_obd_cmd_AUTO,     NULL,              "AUTO",                    },
  {false, NA,    416,  elm_process_obd_cmd_J1850,    NULL,              "SAE J1850 PWM",           },
  {false, NA,    104,  elm_process_obd_cmd_J1850,    NULL,              "SAE J1850 VPW",           },
  {false, LIN,   104,  elm_process_obd_cmd_LIN5baud, NULL,              "ISO 9141-2",              },
  {false, LIN,   104,  elm_process_obd_cmd_LIN5baud, NULL,              "ISO 14230-4 (KWP 5BAUD)", },
  {false, LIN,   104,  elm_process_obd_cmd_LINFast,  NULL,              "ISO 14230-4 (KWP FAST)",  },
  {true,  CAN11, 5000, elm_process_obd_cmd_ISO15765, elm_init_ISO15765, "ISO 15765-4 (CAN 11/500)",},
  {true,  CAN29, 5000, elm_process_obd_cmd_ISO15765, elm_init_ISO15765, "ISO 15765-4 (CAN 29/500)",},
  {true,  CAN11, 2500, elm_process_obd_cmd_ISO15765, elm_init_ISO15765, "ISO 15765-4 (CAN 11/250)",},
  {true,  CAN29, 2500, elm_process_obd_cmd_ISO15765, elm_init_ISO15765, "ISO 15765-4 (CAN 29/250)",},
  {false, CAN29, 2500, elm_process_obd_cmd_CANGen,   NULL,              "SAE J1939 (CAN 29/250)",  },
  {false, CAN11, 1250, elm_process_obd_cmd_CANGen,   NULL,              "USER1 (CAN 11/125)",      },
  {false, CAN11, 500,  elm_process_obd_cmd_CANGen,   NULL,              "USER2 (CAN 11/50)",       },
};

static const elm_protocol_t* ICACHE_FLASH_ATTR elm_current_proto() {
  return &elm_protocols[elm_selected_protocol];
}

/*******************************************
 *** ELM AT command parsing and handling ***
 *******************************************/

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
  {"ST",  2, 4, AT_ST},
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

  os_printf("AT COMMAND ");
  for(int i = 0; i < len; i++) os_printf("%c", cmd[i]);
  os_printf("\r\n");

  switch(elm_parse_at_cmd(cmd, len)){
  case AT_AMP1: //RETURN DEVICE DESCRIPTION
    elm_append_rsp_const(DEVICE_DESC);
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
      elm_append_rsp_const("AUTO, ");
    elm_append_rsp(elm_current_proto()->name,
                   strlen(elm_current_proto()->name));
    elm_append_rsp_const("\r\r");
    return;
  case AT_DPN: //DESCRIBE THE PROTOCOL BY NUMBER
    //TODO: Required. Report currently selected protocol
    if(elm_mode_auto_protocol)
      elm_append_rsp_const("A");
    elm_append_rsp(&hex_lookup[elm_selected_protocol], 1);
    elm_append_rsp_const("\r\r");
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
    elm_append_rsp_const(IDENT_MSG);
    return;
  case AT_L0: //LINEFEED OFF
    elm_mode_linefeed = false;
    break;
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
      elm_append_rsp_const("?\r\r");
      return;
    }
    elm_selected_protocol = tmp;
    elm_mode_auto_protocol = (tmp == 0);
    elm_switch_proto();
    break;
  case AT_SPA: //SET PROTOCOL WITH AUTO FALLBACK
    tmp = elm_decode_hex_char(cmd[3]);
    if(tmp == -1 || tmp >= ELM_PROTOCOL_COUNT) {
      elm_append_rsp_const("?\r\r");
      return;
    }
    elm_selected_protocol = tmp;
    elm_mode_auto_protocol = true;
    elm_switch_proto();
    break;
  case AT_ST:  //SET TIMEOUT
    if(!elm_check_valid_hex_chars(&cmd[2], 2)) {
      elm_append_rsp_const("?\r\r");
      return;
    }

    tmp = elm_decode_hex_byte(&cmd[2]);
    //20 for CAN, 4 for LIN
    elm_mode_timeout = tmp ? tmp*20 : ELM_MODE_TIMEOUT_DEFAULT;
    break;
  case AT_Z: //RESET
    elm_mode_echo = true;
    elm_mode_linefeed = false;
    elm_mode_additional_headers = false;
    elm_mode_auto_protocol = true;
    elm_selected_protocol = ELM_MODE_SELECTED_PROTOCOL_DEFAULT;
    elm_mode_print_spaces = true;
    elm_mode_adaptive_timing = 1;
    elm_mode_allow_long = false;
    elm_mode_timeout = ELM_MODE_TIMEOUT_DEFAULT;

    elm_append_rsp_const("\r\r");
    elm_append_rsp_const(IDENT_MSG);
    panda_set_safety_mode(0xE327);
    elm_switch_proto();
    return;
  default:
    elm_append_rsp_const("?\r\r");
    return;
  }

  elm_append_rsp_const("OK\r\r");
}

/*************************************
 *** Connection and cli management ***
 *************************************/

static void ICACHE_FLASH_ATTR elm_append_in_msg(char *data, uint16_t len) {
  if(in_msg_len + len > sizeof(in_msg))
    len = sizeof(in_msg) - in_msg_len;
  memcpy(in_msg + in_msg_len, data, len);
  in_msg_len += len;
}

static int ICACHE_FLASH_ATTR elm_msg_is_at_cmd(char *data, uint16_t len){
  return len >= 4 && data[0] == 'A' && data[1] == 'T';
}

static void ICACHE_FLASH_ATTR elm_rx_cb(void *arg, char *data, uint16_t len) {
  #ifdef ELM_DEBUG
  os_printf("\nGot ELM Data In: '%s'\n", data);
  #endif

  rsp_buff_len = 0;
  len = elm_msg_find_cr_or_eos(data, len);

  if(loopcount){
    os_timer_disarm(&elm_timeout);
    loopcount = 0;
    got_msg_this_run = false;
    can_tx_worked = false;
    did_multimessage = false;

    os_printf("Interrupting operation, stopping timer. msg len: %d\n", len);
    elm_append_rsp_const("STOPPED\r\r>");
    if(len == 1 && data[0] == '\r') {
      os_printf("Empty msg source of interrupt.\n");
      elm_tcp_tx_flush();
      return;
    }
  }

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
      elm_append_rsp_const(">");
    } else if(elm_check_valid_hex_chars(stripped_msg, stripped_msg_len - 1)) {
      elm_current_proto()->process_obd(elm_current_proto(), stripped_msg, stripped_msg_len);
    } else {
      elm_append_rsp_const("?\r\r>");
    }
  }

  elm_tcp_tx_flush();

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
  //Allow several sends to be queued at a time.
  espconn_tcp_set_buf_count(pesp_conn, 3);

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
