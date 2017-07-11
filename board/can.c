#include <stdint.h>
#include "config.h"
#include "can.h"
#include "uart.h"
#include "gpio.h"
#include "llgpio.h"
#include "libc.h"
#include "rev.h"
#include "safety.h"

int can_live = 0, pending_can_live = 0;

// assign CAN numbering
#ifdef PANDA
  // ********************* instantiate can queues *********************
  can_buffer(rx_q, 0x1000)
  can_buffer(tx1_q, 0x100)
  can_buffer(tx2_q, 0x100)
  can_buffer(tx3_q, 0x100)

  // panda: CAN1 = 0   CAN2 = 1   CAN3 = 2
  can_port_desc can_ports[] = {
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN1,
     .msg_buff = &can_tx1_q,
     .can_pins = {{GPIOB, 8, GPIO_AF8_CAN1}, {GPIOB, 9, GPIO_AF8_CAN1}},
     .enable_pin = {GPIOC, 1, 0},
     .gmlan_support = false,
    },
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN2,
     .msg_buff = &can_tx2_q,
     .can_pins = {{GPIOB, 5, GPIO_AF9_CAN2}, {GPIOB, 6, GPIO_AF9_CAN2}},
     .enable_pin = {GPIOC, 13, 0},
     .gmlan_support = true,
     .gmlan_pins = {{GPIOB, 12, GPIO_AF9_CAN2}, {GPIOB, 13, GPIO_AF9_CAN2}},
    },
    //TODO Make gmlan support correct for REV B
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN3,
     .msg_buff = &can_tx3_q,
     .can_pins = {{GPIOA, 8, GPIO_AF11_CAN3}, {GPIOA, 15, GPIO_AF11_CAN3}},
     .enable_pin = {GPIOA, 0, 0},
     .gmlan_support = true,
     .gmlan_pins = {{GPIOB, 3, GPIO_AF11_CAN3}, {GPIOB, 4, GPIO_AF11_CAN3}},
    }
  };
#else
  // ********************* instantiate can queues *********************
  can_buffer(rx_q, 0x1000)
  can_buffer(tx1_q, 0x100)
  can_buffer(tx2_q, 0x100)
  // old:   CAN1 = 1   CAN2 = 0
  can_port_desc can_ports[] = {
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN2,
     .msg_buff = &can_tx1_q,
     .can_pins = {{GPIOB, 5, GPIO_AF9_CAN2}, {GPIOB, 6, GPIO_AF9_CAN2}},
     .enable_pin = {GPIOB, 4, 1},
     .gmlan_support = false,
    },
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN1,
     .msg_buff = &can_tx2_q,
     .can_pins = {{GPIOB, 8, GPIO_AF9_CAN1}, {GPIOB, 9, GPIO_AF9_CAN1}},
     .enable_pin = {GPIOB, 3, 1},
     .gmlan_support = false,
    }
  };
#endif

int pop(can_ring *q, CAN_FIFOMailBox_TypeDef *elem) {
  if (q->w_ptr != q->r_ptr) {
    *elem = q->elems[q->r_ptr];
    if ((q->r_ptr + 1) == q->fifo_size) q->r_ptr = 0;
    else q->r_ptr += 1;
    return 1;
  }
  return 0;
}

int push(can_ring *q, CAN_FIFOMailBox_TypeDef *elem) {
  uint32_t next_w_ptr;
  if ((q->w_ptr + 1) == q->fifo_size) next_w_ptr = 0;
  else next_w_ptr = q->w_ptr + 1;
  if (next_w_ptr != q->r_ptr) {
    q->elems[q->w_ptr] = *elem;
    q->w_ptr = next_w_ptr;
    return 1;
  }
  puts("push failed!\n");
  return 0;
}

// ********************* CAN Functions *********************

void can_init(uint8_t canid) {
  int i;
  uint32_t bitrate;
  CAN_TypeDef *CAN;
  uint8_t quanta;
  uint16_t prescaler;
  uint8_t seq1, seq2;

  can_port_desc *port;
  gpio_alt_setting *disable_pins;
  gpio_alt_setting *enable_pins;

  puts("Can init: ");
  puth(canid);
  puts("\n");

  if(canid >= CAN_MAX) return;
  port = &can_ports[canid];

  //////////// Set MCU pin modes
  if (port->gmlan_support) {
    disable_pins = port->gmlan ? port->can_pins : port->gmlan_pins;
    enable_pins =  port->gmlan ? port->gmlan_pins : port->can_pins;
  } else {
    disable_pins = 0;
    enable_pins =  port->can_pins;
  }

  // Disable output on either CAN or GMLAN pins
  if (disable_pins) {
    set_gpio_mode(disable_pins[0].port, disable_pins[0].num, MODE_INPUT);
    set_gpio_mode(disable_pins[1].port, disable_pins[1].num, MODE_INPUT);
  }

  // Enable output on either CAN or GMLAN pins
  if (enable_pins) {
    set_gpio_alternate(enable_pins[0].port, enable_pins[0].num, enable_pins[0].setting);
    set_gpio_alternate(enable_pins[1].port, enable_pins[1].num, enable_pins[1].setting);
  }

  /* GMLAN mode pins:
     M0(B15)  M1(B14)  mode
     =======================
     0        0        sleep
     1        0        100kbit
     0        1        high voltage wakeup
     1        1        33kbit (normal)
  */
  if (port->gmlan) {
    set_gpio_output(GPIOB, 14, 1);
    set_gpio_output(GPIOB, 15, 1);
  } else {
    for (i = 0; i < CAN_MAX; i++)
      if (can_ports[i].gmlan)
        break;
    if (i == CAN_MAX){
      set_gpio_output(GPIOB, 14, 0);
      set_gpio_output(GPIOB, 15, 0);
      puts("Disabling GMLAN output\n");
    }
  }

  //////////// Calculate and set CAN bitrate
  if (port->gmlan) {
    bitrate = (port->gmlan_bitrate) < 58333 ? 33333 : 83333;
  } else {
    bitrate = port->bitrate;
    if(bitrate > 1000000) //MAX 1 Megabaud
      bitrate = 1000000;
  }

  //puts("  Speed req: ");
  //puth(bitrate);
  //puts("\n");

  //TODO: Try doing both and find the more accurate values.
  if(min((FREQ / 2) / bitrate, 16) == 16){
    quanta = 16;
    seq1 = 13;//roundf(quanta * 0.875f) - 1;
    seq2 = 2;//roundf(quanta * 0.125f);
  }else{
    quanta = 8;
    seq1 = 6;//roundf(quanta * 0.875f) - 1;
    seq2 = 1;//roundf(quanta * 0.125f);
  }

  // TODO: Look into better accuracy with rounding.
  prescaler = FREQ / quanta / bitrate;

  //Check the prescaler is not larger than max
  if(prescaler > 0x3FF)
    prescaler = 0x3FF;

  bitrate = FREQ/quanta/prescaler;
  if (port->gmlan) {
    port->gmlan_bitrate = bitrate;
  } else {
    port->bitrate = bitrate;
  }

  puts("  Speed: ");
  puth(bitrate);
  puts("\n");

  if (port->gmlan)
    puts("  Type GMLAN\n");
  else
    puts("  Type CAN\n");

  //////////////// Enable CAN port
  if (is_output_enabled())
    puts("  Output Enabled\n");
  else
    puts("  Output Disabled\n");

  set_can_enable(canid, 1);

  CAN = port->CAN;
  // Move CAN to initialization mode and sync.
  CAN->MCR = CAN_MCR_TTCM | CAN_MCR_INRQ;
  while((CAN->MSR & CAN_MSR_INAK) != CAN_MSR_INAK);

  // seg 1: 13 time quanta, seg 2: 2 time quanta
  CAN->BTR = (CAN_BTR_TS1_0 * (seq1 - 1)) |
             (CAN_BTR_TS2_0 * (seq2 - 1)) |
             (prescaler - 1);

  // silent loopback mode for debugging
  #ifdef CAN_LOOPBACK_MODE
    CAN->BTR |= CAN_BTR_SILM | CAN_BTR_LBKM;
  #endif

  if (!is_output_enabled()) {
    CAN->BTR |= CAN_BTR_SILM;
  }

  // reset
  CAN->MCR = CAN_MCR_TTCM;

  int tmp = 0;
  while((CAN->MSR & CAN_MSR_INAK) == CAN_MSR_INAK && tmp < CAN_TIMEOUT) tmp++;

  if (tmp == CAN_TIMEOUT) {
    set_led(LED_BLUE, 1);
    puts("  init FAILED!!!!!\n");
  } else {
    puts("  init SUCCESS\n");
  }

  // accept all filter
  CAN->FMR |= CAN_FMR_FINIT;

  // no mask
  CAN->sFilterRegister[0].FR1 = 0;
  CAN->sFilterRegister[0].FR2 = 0;
  CAN->sFilterRegister[14].FR1 = 0;
  CAN->sFilterRegister[14].FR2 = 0;
  CAN->FA1R |= 1 | (1 << 14);

  CAN->FMR &= ~(CAN_FMR_FINIT);

  // enable all CAN interrupts
  CAN->IER = 0xFFFFFFFF;
  //CAN->IER = CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FMPIE1;
}

void set_can_mode(int canid, int use_gmlan) {
  int i;

  if (canid >= CAN_MAX) return;

  if (use_gmlan)
    for (i = 0; i < CAN_MAX; i++)
      if (can_ports[i].gmlan)
        set_can_mode(i, 0);

  if (!can_ports[canid].gmlan_support) use_gmlan = 0;
  can_ports[canid].gmlan = use_gmlan;

  can_init(canid);
}

// CAN error
void can_sce(uint8_t canid) {
  CAN_TypeDef *CAN = can_ports[canid].CAN;
  #ifdef DEBUG
    if (canid==0) puts("CAN1:  ");
    if (canid==1) puts("CAN2:  ");
    #ifdef CAN3
      if (canid==2) puts("CAN3:  ");
    #endif
    puts("MSR:");
    puth(CAN->MSR);
    puts(" TSR:");
    puth(CAN->TSR);
    puts(" RF0R:");
    puth(CAN->RF0R);
    puts(" RF1R:");
    puth(CAN->RF1R);
    puts(" ESR:");
    puth(CAN->ESR);
    puts("\n");
  #endif

  // clear
  //CAN->sTxMailBox[0].TIR &= ~(CAN_TI0R_TXRQ);
  CAN->TSR |= CAN_TSR_ABRQ0;
  //CAN->ESR |= CAN_ESR_LEC;
  //CAN->MSR &= ~(CAN_MSR_ERRI);
  CAN->MSR = CAN->MSR;
}

void CAN1_SCE_IRQHandler() {
  can_sce(0);
}

void CAN2_SCE_IRQHandler() {
  can_sce(1);
}

void CAN3_SCE_IRQHandler() {
  can_sce(2);
}

// CAN receive handlers
// blink blue when we are receiving CAN messages
void can_rx(int can_index) {
  CAN_TypeDef *CAN = can_ports[can_index].CAN;
  while (CAN->RF0R & CAN_RF0R_FMP0) {
    // can is live
    pending_can_live = 1;

    // add to my fifo
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RIR = CAN->sFIFOMailBox[0].RIR;
    to_push.RDTR = CAN->sFIFOMailBox[0].RDTR;
    to_push.RDLR = CAN->sFIFOMailBox[0].RDLR;
    to_push.RDHR = CAN->sFIFOMailBox[0].RDHR;

    // forwarding (panda only)
    #ifdef PANDA
      if (can_ports[can_index].forwarding != -1) {
        CAN_FIFOMailBox_TypeDef to_send;
        to_send.RIR = to_push.RIR | 1; // TXRQ
        to_send.RDTR = to_push.RDTR;
        to_send.RDLR = to_push.RDLR;
        to_send.RDHR = to_push.RDHR;
        send_can(&to_send, can_ports[can_index].forwarding);
      }
    #endif

    // modify RDTR for our API
    to_push.RDTR = (to_push.RDTR & 0xFFFF000F) | (can_index << 4);

    safety_rx_hook(&to_push);

    #ifdef PANDA
      set_led(LED_GREEN, 1);
    #endif
    push(&can_rx_q, &to_push);

    // next
    CAN->RF0R |= CAN_RF0R_RFOM0;
  }
}

void CAN1_RX0_IRQHandler() {
  can_rx(0);
}

void CAN2_RX0_IRQHandler() {
  can_rx(1);
}

void CAN3_RX0_IRQHandler() {
  can_rx(2);
}

int can_cksum(uint8_t *dat, int len, int addr, int idx) {
  int i;
  int s = 0;
  for (i = 0; i < len; i++) {
    s += (dat[i] >> 4);
    s += dat[i] & 0xF;
  }
  s += (addr>>0)&0xF;
  s += (addr>>4)&0xF;
  s += (addr>>8)&0xF;
  s += idx;
  s = 8-s;
  return s&0xF;
}

void process_can(uint8_t canid) {
  CAN_TypeDef *CAN;

  #ifdef DEBUG
    puts("process CAN TX\n");
  #endif

  if (canid >= CAN_MAX) return;
  CAN = can_ports[canid].CAN;

  // add successfully transmitted message to my fifo
  if ((CAN->TSR & CAN_TSR_TXOK0) == CAN_TSR_TXOK0) {
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RIR = CAN->sTxMailBox[0].TIR;
    to_push.RDTR = (CAN->sTxMailBox[0].TDTR & 0xFFFF000F) |
      ((PANDA_CANB_RETURN_FLAG | (canid & 0x7F)) << 4);
    puts("RDTR: ");
    puth(to_push.RDTR);
    puts("\n");
    to_push.RDLR = CAN->sTxMailBox[0].TDLR;
    to_push.RDHR = CAN->sTxMailBox[0].TDHR;
    push(&can_rx_q, &to_push);
  }

  // check for empty mailbox
  CAN_FIFOMailBox_TypeDef to_send;
  if ((CAN->TSR & CAN_TSR_TME0) == CAN_TSR_TME0) {
    if (pop(can_ports[canid].msg_buff, &to_send)) {
      // only send if we have received a packet
      CAN->sTxMailBox[0].TDLR = to_send.RDLR;
      CAN->sTxMailBox[0].TDHR = to_send.RDHR;
      CAN->sTxMailBox[0].TDTR = to_send.RDTR;
      CAN->sTxMailBox[0].TIR = to_send.RIR;
    }
  }

  // clear interrupt
  CAN->TSR |= CAN_TSR_RQCP0;
}

// send more, possible for these to not trigger?


void CAN1_TX_IRQHandler() {
  process_can(0);
}

void CAN2_TX_IRQHandler() {
  process_can(1);
}

void CAN3_TX_IRQHandler() {
  process_can(2);
}

void send_can(CAN_FIFOMailBox_TypeDef *to_push, int canid) {
  if (canid >= CAN_MAX) return;

  // add CAN packet to send queue
  // bus number isn't passed through
  to_push->RDTR &= 0xF;
  push(can_ports[canid].msg_buff, to_push);

  // canid = can_number
  process_can(canid);
}
