#include <stdint.h>
#include "config.h"
#include "can.h"
#include "uart.h"
#include "gpio.h"
#include "llgpio.h"
#include "libc.h"
#include "rev.h"

// assign CAN numbering
#ifdef PANDA
// panda: CAN1 = 0   CAN2 = 1   CAN3 = 2
  can_port_desc can_ports[] = {
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN1,
     .can_pins = {{GPIOB, 8, GPIO_AF8_CAN1}, {GPIOB, 9, GPIO_AF8_CAN1}},
     .enable_pin = {GPIOC, 1, 0},
     .gmlan_support = false,
    },
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN2,
     .can_pins = {{GPIOB, 5, GPIO_AF9_CAN2}, {GPIOB, 6, GPIO_AF9_CAN2}},
     .enable_pin = {GPIOC, 13, 0},
     .gmlan_support = true,
     .gmlan_pins = {{GPIOB, 12, GPIO_AF9_CAN2}, {GPIOB, 13, GPIO_AF9_CAN2}},
    },
    //TODO Make gmlan support correct for REV B
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN3,
     .can_pins = {{GPIOA, 8, GPIO_AF11_CAN3}, {GPIOA, 15, GPIO_AF11_CAN3}},
     .enable_pin = {GPIOA, 0, 0},
     .gmlan_support = true,
     .gmlan_pins = {{GPIOB, 3, GPIO_AF11_CAN3}, {GPIOB, 4, GPIO_AF11_CAN3}},
    }
  };
#else
// old:   CAN1 = 1   CAN2 = 0
  can_port_desc can_ports[] = {
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN2,
     .can_pins = {{GPIOB, 5, GPIO_AF9_CAN2}, {GPIOB, 6, GPIO_AF9_CAN2}},
     .enable_pin = {GPIOB, 4, 1},
     .gmlan_support = false,
    },
    {CAN_PORT_DESC_INITIALIZER,
     .CAN = CAN1,
     .can_pins = {{GPIOB, 8, GPIO_AF9_CAN1}, {GPIOB, 9, GPIO_AF9_CAN1}},
     .enable_pin = {GPIOB, 3, 1},
     .gmlan_support = false,
    }
  };
#endif

int controls_allowed = 0;

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
  if (controls_allowed)
    puts("  Output Enabled\n");
  else
    puts("  Output Disabled\n");

  //TODO: Eddie, check if disabling output causes correct behavior.
  //set_can_enable(canid, controls_allowed);
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

  if (!controls_allowed) {
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
void can_sce(CAN_TypeDef *CAN) {
  #ifdef DEBUG
    if (CAN==CAN1) puts("CAN1:  ");
    if (CAN==CAN2) puts("CAN2:  ");
    #ifdef CAN3
      if (CAN==CAN3) puts("CAN3:  ");
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
