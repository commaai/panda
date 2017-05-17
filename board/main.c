//#define DEBUG
//#define DEBUG_USB
//#define CAN_LOOPBACK_MODE
//#define USE_INTERNAL_OSC
//#define REVC

#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
#else
  #include "stm32f2xx.h"
#endif

#ifdef PANDA
  #define ENABLE_CURRENT_SENSOR
  #define ENABLE_SPI
#endif

#define USB_VID 0xbbaa
#define USB_PID 0xddcc

#define NULL ((void*)0)

#include "early.h"

// assign CAN numbering
// old:         CAN1 = 1   CAN2 = 0
// panda:       CAN1 = 0   CAN2 = 1   CAN3 = 4
#ifdef PANDA
  int can_numbering[] = {0,1,4};
#else
  int can_numbering[] = {1,0,-1};
#endif

// *** end config ***

#include "obj/gitversion.h"

// debug safety check: is controls allowed?
int controls_allowed = 0;
int started = 0;
int can_live = 0, pending_can_live = 0;

// optional features
int gas_interceptor_detected = 0;
int started_signal_detected = 0;

// detect high on UART
// TODO: check for UART high
int did_usb_enumerate = 0;

// ********************* instantiate queues *********************

typedef struct {
  uint32_t w_ptr;
  uint32_t r_ptr;
  uint32_t fifo_size;
  CAN_FIFOMailBox_TypeDef *elems;
} can_ring;

#define can_buffer(x, size) \
  CAN_FIFOMailBox_TypeDef elems_##x[size]; \
  can_ring can_##x = { .w_ptr = 0, .r_ptr = 0, .fifo_size = size, .elems = (CAN_FIFOMailBox_TypeDef *)&elems_##x };

can_buffer(rx_q, 0x1000)
can_buffer(tx1_q, 0x100)
can_buffer(tx2_q, 0x100)
can_buffer(tx3_q, 0x100)

// ********************* interrupt safe queue *********************

inline int pop(can_ring *q, CAN_FIFOMailBox_TypeDef *elem) {
  if (q->w_ptr != q->r_ptr) {
    *elem = q->elems[q->r_ptr];
    if ((q->r_ptr + 1) == q->fifo_size) q->r_ptr = 0;
    else q->r_ptr += 1;
    return 1;
  }
  return 0;
}

inline int push(can_ring *q, CAN_FIFOMailBox_TypeDef *elem) {
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

// ***************************** serial port queues *****************************

#define FIFO_SIZE 0x100

typedef struct uart_ring {
  uint8_t w_ptr_tx;
  uint8_t r_ptr_tx;
  uint8_t elems_tx[FIFO_SIZE];
  uint8_t w_ptr_rx;
  uint8_t r_ptr_rx;
  uint8_t elems_rx[FIFO_SIZE];
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
} uart_ring;

inline int getc(uart_ring *q, char *elem);
inline int putc(uart_ring *q, char elem);

// esp = USART1
uart_ring esp_ring = { .w_ptr_tx = 0, .r_ptr_tx = 0,
                       .w_ptr_rx = 0, .r_ptr_rx = 0,
                       .uart = USART1 };

// lin1, K-LINE = UART5
// lin2, L-LINE = USART3
uart_ring lin1_ring = { .w_ptr_tx = 0, .r_ptr_tx = 0,
                        .w_ptr_rx = 0, .r_ptr_rx = 0,
                        .uart = UART5 };
uart_ring lin2_ring = { .w_ptr_tx = 0, .r_ptr_tx = 0,
                        .w_ptr_rx = 0, .r_ptr_rx = 0,
                        .uart = USART3 };

// debug = USART2
void debug_ring_callback(uart_ring *ring);
uart_ring debug_ring = { .w_ptr_tx = 0, .r_ptr_tx = 0,
                         .w_ptr_rx = 0, .r_ptr_rx = 0,
                         .uart = USART2,
                         .callback = debug_ring_callback};


uart_ring *get_ring_by_number(int a) {
  switch(a) {
    case 0:
      return &debug_ring;
    case 1:
      return &esp_ring;
    case 2:
      return &lin1_ring;
    case 3:
      return &lin2_ring;
    default:
      return NULL;
  }
}

void accord_framing_callback(uart_ring *q) {
  uint8_t r_ptr_rx_tmp = q->r_ptr_rx;
  int sof1 = -1;
  int sof2 = -1;
  int i;
  char junk;
  int jlen = 0;
  int plen = 0;
  while (q->w_ptr_rx != r_ptr_rx_tmp) {
    if ((q->elems_rx[r_ptr_rx_tmp] & 0x80) == 0) {
      if (sof1 == -1) {
        sof1 = r_ptr_rx_tmp;
      } else {
        sof2 = r_ptr_rx_tmp;
        break;
      }
    }
    if (sof1 != -1) {
      plen++;
    } else {
      jlen++;
    }
    r_ptr_rx_tmp++;
  }
  
  // drop until SOF1
  if (sof1 != -1) {
    for (i = 0; i < jlen; i++) getc(q, &junk);
  }

  if (sof2 != -1) {
    //puth(sof1); puts(" "); puth(sof2); puts("\n");

    if (plen > 8) {
      // drop oversized packet
      for (i = 0; i < plen; i++) getc(q, &junk);
    } else {
      // packet received
      CAN_FIFOMailBox_TypeDef to_push;
      to_push.RIR = 0;
      to_push.RDTR = plen;
      to_push.RDLR = 0;
      to_push.RDHR = 0;

      // 8 is K-line, 9 is L-line
      if (q->uart == UART5) {
        to_push.RDTR |= 8 << 4;
      } else if (q->uart == USART3) {
        to_push.RDTR |= 9 << 4;
      }

      // get data from queue
      for (i = 0; i < plen; i++) {
        getc(q, &(((char*)(&to_push.RDLR))[i]));
      }

      push(&can_rx_q, &to_push);
    }
  }
}

void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (getc(ring, &rcv)) {
    putc(ring, rcv);

    // jump to DFU flash
    if (rcv == 'z') {
      enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
      NVIC_SystemReset();
    }
    if (rcv == 'x') {
      // normal reset
      NVIC_SystemReset();
    }
  }
}

// ***************************** serial port *****************************

void uart_ring_process(uart_ring *q) {
  // TODO: check if external serial is connected
  int sr = q->uart->SR;

  if (q->w_ptr_tx != q->r_ptr_tx) {
    if (sr & USART_SR_TXE) {
      q->uart->DR = q->elems_tx[q->r_ptr_tx];
      q->r_ptr_tx += 1;
    } else {
      // push on interrupt later
      q->uart->CR1 |= USART_CR1_TXEIE;
    }
  } else {
    // nothing to send
    q->uart->CR1 &= ~USART_CR1_TXEIE;
  }

  if (sr & USART_SR_RXNE) {
    uint8_t c = q->uart->DR;  // TODO: can drop packets
    uint8_t next_w_ptr = q->w_ptr_rx + 1;
    if (next_w_ptr != q->r_ptr_rx) {
      q->elems_rx[q->w_ptr_rx] = c;
      q->w_ptr_rx = next_w_ptr;
      if (q->callback) q->callback(q);
    }
  }
}

// interrupt boilerplate

void USART1_IRQHandler(void) {
  NVIC_DisableIRQ(USART1_IRQn);
  uart_ring_process(&esp_ring);
  NVIC_EnableIRQ(USART1_IRQn);
}

void USART2_IRQHandler(void) {
  NVIC_DisableIRQ(USART2_IRQn);
  uart_ring_process(&debug_ring);
  NVIC_EnableIRQ(USART2_IRQn);
}

void USART3_IRQHandler(void) {
  NVIC_DisableIRQ(USART3_IRQn);
  uart_ring_process(&lin2_ring);
  NVIC_EnableIRQ(USART3_IRQn);
}

void UART5_IRQHandler(void) {
  NVIC_DisableIRQ(UART5_IRQn);
  uart_ring_process(&lin1_ring);
  NVIC_EnableIRQ(UART5_IRQn);
}

inline int getc(uart_ring *q, char *elem) {
  if (q->w_ptr_rx != q->r_ptr_rx) {
    *elem = q->elems_rx[q->r_ptr_rx];
    q->r_ptr_rx += 1;
    return 1;
  }
  return 0;
}

inline int injectc(uart_ring *q, char elem) {
  uint8_t next_w_ptr = q->w_ptr_rx + 1;
  int ret = 0;
  if (next_w_ptr != q->r_ptr_rx) {
    q->elems_rx[q->w_ptr_rx] = elem;
    q->w_ptr_rx = next_w_ptr;
    ret = 1;
  }
  return ret;
}

inline int putc(uart_ring *q, char elem) {
  uint8_t next_w_ptr = q->w_ptr_tx + 1;
  int ret = 0;
  if (next_w_ptr != q->r_ptr_tx) {
    q->elems_tx[q->w_ptr_tx] = elem;
    q->w_ptr_tx = next_w_ptr;
    ret = 1;
  }
  uart_ring_process(q);
  return ret;
}

// ********************* includes *********************

#include "libc.h"
#include "gpio.h"
#include "uart.h"
#include "adc.h"
#include "timer.h"
#include "usb.h"
#include "can.h"
#include "spi.h"

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired);

#ifdef PANDA_SAFETY
#include "panda_safety.h"
#else
#include "honda_safety.h"
#endif

// ***************************** CAN *****************************

void process_can(CAN_TypeDef *CAN, can_ring *can_q, int can_number) {
  #ifdef DEBUG
    puts("process CAN TX\n");
  #endif

  // add successfully transmitted message to my fifo
  if ((CAN->TSR & CAN_TSR_TXOK0) == CAN_TSR_TXOK0) {
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RIR = CAN->sTxMailBox[0].TIR;
    to_push.RDTR = (CAN->sTxMailBox[0].TDTR & 0xFFFF000F) | ((can_number+2) << 4);
    to_push.RDLR = CAN->sTxMailBox[0].TDLR;
    to_push.RDHR = CAN->sTxMailBox[0].TDHR;
    push(&can_rx_q, &to_push);
  }

  // check for empty mailbox
  CAN_FIFOMailBox_TypeDef to_send;
  if ((CAN->TSR & CAN_TSR_TME0) == CAN_TSR_TME0) {
    if (pop(can_q, &to_send)) {
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
  process_can(CAN1, &can_tx1_q, can_numbering[0]);
}

void CAN2_TX_IRQHandler() {
  process_can(CAN2, &can_tx2_q, can_numbering[1]);
}

#ifdef CAN3
void CAN3_TX_IRQHandler() {
  process_can(CAN3, &can_tx3_q, can_numbering[2]);
}
#endif

// CAN receive handlers
// blink blue when we are receiving CAN messages
void can_rx(CAN_TypeDef *CAN, int can_number) {
  while (CAN->RF0R & CAN_RF0R_FMP0) {
    // can is live
    pending_can_live = 1;

    // add to my fifo
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RIR = CAN->sFIFOMailBox[0].RIR;
    // top 16-bits is the timestamp
    to_push.RDTR = (CAN->sFIFOMailBox[0].RDTR & 0xFFFF000F) | (can_number << 4);
    to_push.RDLR = CAN->sFIFOMailBox[0].RDLR;
    to_push.RDHR = CAN->sFIFOMailBox[0].RDHR;

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
  //puts("CANRX1");
  //delay(10000);
  can_rx(CAN1, can_numbering[0]);
}

void CAN2_RX0_IRQHandler() {
  //puts("CANRX0");
  //delay(10000);
  can_rx(CAN2, can_numbering[1]);
}

#ifdef CAN3
void CAN3_RX0_IRQHandler() {
  //puts("CANRX0");
  //delay(10000);
  can_rx(CAN3, can_numbering[2]);
}
#endif

void CAN1_SCE_IRQHandler() {
  //puts("CAN1_SCE\n");
  can_sce(CAN1);
}

void CAN2_SCE_IRQHandler() {
  //puts("CAN2_SCE\n");
  can_sce(CAN2);
}

#ifdef CAN3
void CAN3_SCE_IRQHandler() {
  //puts("CAN3_SCE\n");
  can_sce(CAN3);
}
#endif


// ***************************** USB port *****************************

int get_health_pkt(void *dat) {
  struct __attribute__((packed)) {
    uint32_t voltage;
    uint32_t current;
    uint8_t started;
    uint8_t controls_allowed;
    uint8_t gas_interceptor_detected;
    uint8_t started_signal_detected;
    uint8_t started_alt;
  } *health = dat;
  health->voltage = adc_get(ADCCHAN_VOLTAGE);
#ifdef ENABLE_CURRENT_SENSOR
  health->current = adc_get(ADCCHAN_CURRENT);
#else
  health->current = 0;
#endif
  health->started = started;

#ifdef PANDA
  health->started_alt = (GPIOA->IDR & (1 << 1)) == 0;
#else
  health->started_alt = 0;
#endif

  health->controls_allowed = controls_allowed;

  health->gas_interceptor_detected = gas_interceptor_detected;
  health->started_signal_detected = started_signal_detected;
  return sizeof(*health);
}

void set_fan_speed(int fan_speed) {
  TIM3->CCR3 = fan_speed;
}

int usb_cb_ep1_in(uint8_t *usbdata, int len, int hardwired) {
  CAN_FIFOMailBox_TypeDef *reply = (CAN_FIFOMailBox_TypeDef *)usbdata;;

  int ilen = 0;
  while (ilen < min(len/0x10, 4) && pop(&can_rx_q, &reply[ilen])) ilen++;

  return ilen*0x10;
}

// send on serial, first byte to select
void usb_cb_ep2_out(uint8_t *usbdata, int len, int hardwired) {
  int i;
  if (len == 0) return;
  uart_ring *ur = get_ring_by_number(usbdata[0]);
  if (!ur) return;
  if ((usbdata[0] < 2) || safety_tx_lin_hook(usbdata[0]-2, usbdata+1, len-1, hardwired)) {
    for (i = 1; i < len; i++) while (!putc(ur, usbdata[i]));
  }
}

// send on CAN
void usb_cb_ep3_out(uint8_t *usbdata, int len, int hardwired) {
  int dpkt = 0;
  int i;
  for (dpkt = 0; dpkt < len; dpkt += 0x10) {
    uint32_t *tf = (uint32_t*)(&usbdata[dpkt]);

    // make a copy
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RDHR = tf[3];
    to_push.RDLR = tf[2];
    to_push.RDTR = tf[1];
    to_push.RIR = tf[0];

    int flags = (to_push.RDTR >> 4) & 0xF;
    if (safety_tx_hook(&to_push, hardwired)) {
      CAN_TypeDef *CAN;
      can_ring *can_q;
      if (flags == can_numbering[0])  {
        CAN = CAN1;
        can_q = &can_tx1_q;
      } else if (flags == can_numbering[1]) {
        CAN = CAN2;
        can_q = &can_tx2_q;
      #ifdef CAN3
      } else if (flags == can_numbering[2]) {
        CAN = CAN3;
        can_q = &can_tx3_q;
      #endif
      } else if (flags == 8 || flags == 9) {
        // fake LIN as CAN
        uart_ring *lin_ring = (flags == 8) ? &lin1_ring : &lin2_ring;
        for (i = 0; i < min(8, to_push.RDTR & 0xF); i++) {
          putc(lin_ring, ((uint8_t*)&to_push.RDLR)[i]);
        }
        continue;
      } else {
        // no crash
        continue;
      }

      // add CAN packet to send queue
      // bus number isn't passed through
      to_push.RDTR &= 0xF;
      push(can_q, &to_push);

      // flags = can_number
      process_can(CAN, can_q, flags);
    }
  }
}

void usb_cb_enumeration_complete() {
  // power down the ESP
  // this doesn't work and makes the board unflashable
  // because the ESP spews shit on serial on startup
  //GPIOC->ODR &= ~(1 << 14);
  did_usb_enumerate = 1;
}

int usb_cb_control_msg(USB_Setup_TypeDef *setup, uint8_t *resp, int hardwired) {
  int resp_len = 0;
  uart_ring *ur = NULL;
  int i;
  switch (setup->b.bRequest) {
    case 0xd0:
      // fetch serial number
      #ifdef PANDA
        // addresses are OTP
        if (setup->b.wValue.w == 1) {
          memcpy(resp, (void *)0x1fff79c0, 0x10);
          resp_len = 0x10;
        } else {
          memcpy(resp, (void *)0x1fff79e0, 0x20);
          resp_len = 0x20;
        }
      #endif
      break;
    case 0xd1:
      if (hardwired) {
        enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
        NVIC_SystemReset();
      }
      break;
    case 0xd2:
      resp_len = get_health_pkt(resp);
      break;
    case 0xd3:
      set_fan_speed(setup->b.wValue.w);
      break;
    case 0xd6: // GET_VERSION
      // assert(sizeof(gitversion) <= MAX_RESP_LEN);
      memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = sizeof(gitversion);
      break;
    case 0xd8: // RESET
      NVIC_SystemReset();
      break;
    case 0xd9: // ESP SET POWER
      if (setup->b.wValue.w == 1) {
        // on
        GPIOC->ODR |= (1 << 14);
      } else {
        // off
        GPIOC->ODR &= ~(1 << 14);
      }
      break;
    case 0xda: // ESP RESET
      // pull low for ESP boot mode
      if (setup->b.wValue.w == 1) {
        GPIOC->ODR &= ~(1 << 5);
      }

      // do ESP reset
      GPIOC->ODR &= ~(1 << 14);
      delay(1000000);
      GPIOC->ODR |= (1 << 14);
      delay(1000000);

      // reset done, no more boot mode
      // TODO: ESP doesn't seem to listen here
      if (setup->b.wValue.w == 1) {
        GPIOC->ODR |= (1 << 5);
      }
      break;
    case 0xdb: // toggle GMLAN
      set_can2_mode(setup->b.wValue.w);
      break;
    case 0xdc: // set controls allowed
      controls_allowed = setup->b.wValue.w == 0x1337;
      break;
    case 0xe0: // uart read
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      // read
      while (resp_len < min(setup->b.wLength.w, MAX_RESP_LEN) && getc(ur, (char*)&resp[resp_len])) {
        ++resp_len;
      }
      break;
    case 0xe1: // uart set baud rate
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      uart_set_baud(ur->uart, setup->b.wIndex.w);
      break;
    case 0xe2: // uart set parity
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      switch (setup->b.wIndex.w) {
        case 0:
          // disable parity, 8-bit
          ur->uart->CR1 &= ~(USART_CR1_PCE | USART_CR1_M);
          break;
        case 1:
          // even parity, 9-bit
          ur->uart->CR1 &= ~USART_CR1_PS;
          ur->uart->CR1 |= USART_CR1_PCE | USART_CR1_M;
          break;
        case 2:
          // odd parity, 9-bit
          ur->uart->CR1 |= USART_CR1_PS;
          ur->uart->CR1 |= USART_CR1_PCE | USART_CR1_M;
          break;
        default:
          break;
      }
      break;
    case 0xe3: // uart install accord framing callback
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      if (setup->b.wIndex.w == 1) {
        ur->callback = accord_framing_callback;
      } else {
        ur->callback = NULL;
      }
      break;
    case 0xf0: // k-line wValue pulse on uart2
      if (setup->b.wValue.w == 1) {
        GPIOC->ODR &= ~(1 << 10);
        GPIOC->MODER &= ~GPIO_MODER_MODER10_1;
        GPIOC->MODER |= GPIO_MODER_MODER10_0;
      } else {
        GPIOC->ODR &= ~(1 << 12);
        GPIOC->MODER &= ~GPIO_MODER_MODER12_1;
        GPIOC->MODER |= GPIO_MODER_MODER12_0;
      }

      for (i = 0; i < 80; i++) {
        delay(8000);
        if (setup->b.wValue.w == 1) {
          GPIOC->ODR |= (1 << 10);
          GPIOC->ODR &= ~(1 << 10);
        } else {
          GPIOC->ODR |= (1 << 12);
          GPIOC->ODR &= ~(1 << 12);
        }
      }

      if (setup->b.wValue.w == 1) {
        GPIOC->MODER &= ~GPIO_MODER_MODER10_0;
        GPIOC->MODER |= GPIO_MODER_MODER10_1;
      } else {
        GPIOC->MODER &= ~GPIO_MODER_MODER12_0;
        GPIOC->MODER |= GPIO_MODER_MODER12_1;
      }

      delay(140 * 9000);
      break;
    default:
      puts("NO HANDLER ");
      puth(setup->b.bRequest);
      puts("\n");
      break;
  }
  return resp_len;
}


void OTG_FS_IRQHandler(void) {
  NVIC_DisableIRQ(OTG_FS_IRQn);
  //__disable_irq();
  usb_irqhandler();
  //__enable_irq();
  NVIC_EnableIRQ(OTG_FS_IRQn);
}

void ADC_IRQHandler(void) {
  puts("ADC_IRQ\n");
}

#ifdef ENABLE_SPI

#define SPI_BUF_SIZE 256
uint8_t spi_buf[SPI_BUF_SIZE];
int spi_buf_count = 0;
int spi_total_count = 0;
uint8_t spi_tx_buf[0x44];

void handle_spi(uint8_t *data, int len) {
  memset(spi_tx_buf, 0xaa, 0x44);
  // data[0]  = endpoint
  // data[2]  = length
  // data[4:] = data
  int *resp_len = (int*)spi_tx_buf;
  *resp_len = 0;
  switch (data[0]) {
    case 0:
      // control transfer
      *resp_len = usb_cb_control_msg((USB_Setup_TypeDef *)(data+4), spi_tx_buf+4, 0);
      break;
    case 1:
      // ep 1, read
      *resp_len = usb_cb_ep1_in(spi_tx_buf+4, 0x40, 0);
      break;
    case 2:
      // ep 2, send serial
      usb_cb_ep2_out(data+4, data[2], 0);
      break;
    case 3:
      // ep 3, send CAN
      usb_cb_ep3_out(data+4, data[2], 0);
      break;
  }
  spi_tx_dma(spi_tx_buf, 0x44);

  // signal data is ready by driving low
  // esp must be configured as input by this point
  GPIOB->MODER &= ~(GPIO_MODER_MODER0);
  GPIOB->MODER |= GPIO_MODER_MODER0_0;
  GPIOB->ODR &= ~(GPIO_ODR_ODR_0);
}

// SPI RX
void DMA2_Stream2_IRQHandler(void) {
  // ack
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;
  handle_spi(spi_buf, 0x14);
}

// SPI TX
void DMA2_Stream3_IRQHandler(void) {
  // ack
  DMA2->LIFCR = DMA_LIFCR_CTCIF3;

  // reset handshake back to pull up
  GPIOB->MODER &= ~(GPIO_MODER_MODER0);
  GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0;
}

void EXTI4_IRQHandler(void) {
  int pr = EXTI->PR;
  // SPI CS rising
  if (pr & (1 << 4)) {
    spi_total_count = 0;
    spi_rx_dma(spi_buf, 0x14);
    //puts("exti4\n");
  }
  EXTI->PR = pr;
}

#endif

// ***************************** main code *****************************

void __initialize_hardware_early() {
  early();
}

int main() {
  // init devices
  clock_init();
  periph_init();

  detect();
  gpio_init();

  // enable main uart
  uart_init(USART2, 115200);

  // enable ESP uart
  uart_init(USART1, 115200);

  // enable LIN
  uart_init(UART5, 10400);
  UART5->CR2 |= USART_CR2_LINEN;
  uart_init(USART3, 10400);
  USART3->CR2 |= USART_CR2_LINEN;

  /*puts("EXTERNAL");
  puth(has_external_debug_serial);
  puts("\n");*/

  // enable USB
  usb_init();

  can_init(CAN1);
  can_init(CAN2);
#ifdef CAN3
  can_init(CAN3);
#endif
  adc_init();

#ifdef ENABLE_SPI
  spi_init();
#endif

  // timer for fan PWM
  TIM3->CCMR2 = TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;
  TIM3->CCER = TIM_CCER_CC3E;

  // max value of the timer
  // 64 makes it above the audible range
  //TIM3->ARR = 64;

  // 10 prescale makes it below the audible range
  timer_init(TIM3, 10);
  puth(DBGMCU->IDCODE);

  // set PWM
  set_fan_speed(65535);


  puts("**** INTERRUPTS ON ****\n");
  __disable_irq();

  // 4 uarts!
  NVIC_EnableIRQ(USART1_IRQn);
  NVIC_EnableIRQ(USART2_IRQn);
  NVIC_EnableIRQ(USART3_IRQn);
  NVIC_EnableIRQ(UART5_IRQn);

  NVIC_EnableIRQ(OTG_FS_IRQn);
  NVIC_EnableIRQ(ADC_IRQn);
  // CAN has so many interrupts!

  NVIC_EnableIRQ(CAN1_TX_IRQn);
  NVIC_EnableIRQ(CAN1_RX0_IRQn);
  NVIC_EnableIRQ(CAN1_SCE_IRQn);

  NVIC_EnableIRQ(CAN2_TX_IRQn);
  NVIC_EnableIRQ(CAN2_RX0_IRQn);
  NVIC_EnableIRQ(CAN2_SCE_IRQn);

#ifdef CAN3
  NVIC_EnableIRQ(CAN3_TX_IRQn);
  NVIC_EnableIRQ(CAN3_RX0_IRQn);
  NVIC_EnableIRQ(CAN3_SCE_IRQn);
#endif

#ifdef ENABLE_SPI
  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  //NVIC_EnableIRQ(SPI1_IRQn);

  // setup interrupt on falling edge of SPI enable (on PA4)
  SYSCFG->EXTICR[2] = SYSCFG_EXTICR2_EXTI4_PA;
  EXTI->IMR = (1 << 4);
  EXTI->FTSR = (1 << 4);
  NVIC_EnableIRQ(EXTI4_IRQn);
#endif
  __enable_irq();

  puts("OPTCR: "); puth(FLASH->OPTCR); puts("\n");

  // LED should keep on blinking all the time
  uint64_t cnt;
  for (cnt=0;;cnt++) {
    can_live = pending_can_live;

    // reset this every 16th pass
    if ((cnt&0xF) == 0) pending_can_live = 0;

    /*#ifdef DEBUG
      puts("** blink ");
      puth(can_rx_q.r_ptr); puts(" "); puth(can_rx_q.w_ptr); puts("  ");
      puth(can_tx1_q.r_ptr); puts(" "); puth(can_tx1_q.w_ptr); puts("  ");
      puth(can_tx2_q.r_ptr); puts(" "); puth(can_tx2_q.w_ptr); puts("\n");
    #endif*/

    /*puts("voltage: "); puth(adc_get(ADCCHAN_VOLTAGE)); puts("  ");
    puts("current: "); puth(adc_get(ADCCHAN_CURRENT)); puts("\n");*/

    // set LED to be controls allowed, blue on panda, green on legacy
    #ifdef PANDA
      set_led(LED_BLUE, controls_allowed);
    #else
      set_led(LED_GREEN, controls_allowed);
    #endif

    // blink the red LED
    set_led(LED_RED, 0);
    delay(2000000);
    set_led(LED_RED, 1);
    delay(2000000);

    // turn off the green LED, turned on by CAN
    #ifdef PANDA
      set_led(LED_GREEN, 0);
    #endif

    #ifdef ENABLE_SPI
      /*if (spi_buf_count > 0) {
        hexdump(spi_buf, spi_buf_count);
        spi_buf_count = 0;
      }*/
    #endif

    // started logic
    #ifdef PANDA
      int started_signal = (GPIOB->IDR & (1 << 12)) == 0;
    #else
      int started_signal = (GPIOC->IDR & (1 << 13)) != 0;
    #endif
    if (started_signal) { started_signal_detected = 1; }

    if (started_signal || (!started_signal_detected && can_live)) {
      started = 1;

      // turn on fan at half speed
      set_fan_speed(32768);
    } else {
      started = 0;

      // turn off fan
      set_fan_speed(0);
    }
  }

  return 0;
}

