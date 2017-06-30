#include "config.h"
#include "early.h"
#include "obj/gitversion.h"

// debug safety check: is controls allowed?
int started = 0;
int can_live = 0, pending_can_live = 0;

// optional features
int gas_interceptor_detected = 0;
int started_signal_detected = 0;

// detect high on UART
// TODO: check for UART high
int did_usb_enumerate = 0;


#include "uart.h"
#include "can.h"

// ********************* instantiate can queues *********************

can_buffer(rx_q, 0x1000)
can_buffer(tx1_q, 0x100)
can_buffer(tx2_q, 0x100)
can_buffer(tx3_q, 0x100)

// ***************************** serial port queues *****************************

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

// ********************* includes *********************
#include "libc.h"
#include "gpio.h"
#include "adc.h"
#include "timer.h"
#include "usb.h"
#include "spi.h"

void safety_rx_hook(CAN_FIFOMailBox_TypeDef *to_push);
int safety_tx_hook(CAN_FIFOMailBox_TypeDef *to_send, int hardwired);
int safety_tx_lin_hook(int lin_num, uint8_t *data, int len, int hardwired);

#ifdef PANDA_SAFETY
#include "panda_safety.h"
#else
#include "honda_safety.h"
#endif

#define PANDA_CANB_RETURN_FLAG 0x80

// ***************************** CAN *****************************

void process_can(CAN_TypeDef *CAN, can_ring *can_q, int can_number) {
  #ifdef DEBUG
    puts("process CAN TX\n");
  #endif

  // add successfully transmitted message to my fifo
  if ((CAN->TSR & CAN_TSR_TXOK0) == CAN_TSR_TXOK0) {
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RIR = CAN->sTxMailBox[0].TIR;
    to_push.RDTR = (CAN->sTxMailBox[0].TDTR & 0xFFFF000F) |
      ((PANDA_CANB_RETURN_FLAG | (can_number & 0x7F)) << 4);
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
  process_can(can_ports[0].CAN, &can_tx1_q, 0);
}

void CAN2_TX_IRQHandler() {
  process_can(can_ports[1].CAN, &can_tx2_q, 1);
}

#ifdef PANDA
void CAN3_TX_IRQHandler() {
  process_can(can_ports[2].CAN, &can_tx3_q, 2);
}
#endif

void send_can(CAN_FIFOMailBox_TypeDef *to_push, int flags);

// CAN receive handlers
// blink blue when we are receiving CAN messages
void can_rx(CAN_TypeDef *CAN, int can_index) {
  //int can_number = can_ports[can_index].CAN;
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
  //puts("CANRX1");
  //delay(10000);
  can_rx(CAN1, 0);
}

void CAN2_RX0_IRQHandler() {
  //puts("CANRX0");
  //delay(10000);
  can_rx(CAN2, 1);
}

#ifdef CAN3
void CAN3_RX0_IRQHandler() {
  //puts("CANRX0");
  //delay(10000);
  can_rx(CAN3, 2);
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

void usb_cb_ep0_out(uint8_t *usbdata, int len, int hardwired) {
  if(setup.b.bRequest == 0xde){
    puts("Setting baud rate from usb\n");
    uint32_t bitrate = *(int*)usbdata;
    uint16_t canb_id = setup.b.wValue.w;

    can_ports[canb_id].bitrate = bitrate;
    can_init(canb_id);
  }
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

void send_can(CAN_FIFOMailBox_TypeDef *to_push, int flags) {
  int i;
  can_ring *can_q;
  uart_ring *lin_ring;
  CAN_TypeDef *CAN = can_ports[flags].CAN;
  switch(flags){
  case 0:
    can_q = &can_tx1_q;
    break;
  case 1:
    can_q = &can_tx2_q;
    break;
  #ifdef CAN3
  case 2:
    can_q = &can_tx3_q;
    break;
  #endif
  case 8:
  case 9:
    // fake LIN as CAN
    lin_ring = (flags == 8) ? &lin1_ring : &lin2_ring;
    for (i = 0; i < min(8, to_push->RDTR & 0xF); i++) {
      putc(lin_ring, ((uint8_t*)&to_push->RDLR)[i]);
    }
    return;
  default:
    // no crash
    return;
  }

  // add CAN packet to send queue
  // bus number isn't passed through
  to_push->RDTR &= 0xF;
  push(can_q, to_push);

  // flags = can_number
  process_can(CAN, can_q, flags);
}

// send on CAN
void usb_cb_ep3_out(uint8_t *usbdata, int len, int hardwired) {
  puts("usb_cb_ep3_out called\n");
  int dpkt = 0;
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
      send_can(&to_push, flags);
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
      if (setup->b.wIndex.w == 3) {
        set_can_mode(2, 0);
        set_can_mode(3, setup->b.wValue.w);
      } else {
        set_can_mode(3, 0);
        set_can_mode(2, setup->b.wValue.w);
      }
      break;
    case 0xdc: // set controls allowed
      controls_allowed = setup->b.wValue.w == 0x1337;
      // take CAN out of SILM, careful with speed!
      can_init(0);
      can_init(1);
      #ifdef CAN3
      can_init(2);
      #endif
      break;
    case 0xdd: // enable can forwarding
      //TODO: standardize canid
      if (setup->b.wValue.w != 0 && setup->b.wValue.w <= CAN_MAX) {
        // 0 sets it to -1
        if (setup->b.wIndex.w <= CAN_MAX) {
          can_ports[setup->b.wValue.w-1].forwarding = setup->b.wIndex.w-1;
        }
      }
      break;
    case 0xde: // Set Can bitrate
      puts("Set can bitrate\n");
      if (!(setup->b.wValue.w < CAN_MAX && setup->b.wLength.w == 4)) {
	return -1;
      }
      break;
    case 0xdf: // Set Can bitrate
      puts("Get can bitrate\n");
      if (setup->b.wValue.w < CAN_MAX) {
	//TODO: Make fail if asking for can3 and no can3
	puts("Canid: ");
	puth(setup->b.wValue.w);
	puts(" bitrate: ");
	puth(can_ports[setup->b.wValue.w].bitrate);
	puts("\n");
	memcpy(resp, (void *)&can_ports[setup->b.wValue.w].bitrate, 4);
	resp_len = 4;
      }else{
	return -1;
      }
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
    case 0xe4: // uart set baud rate extended
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      uart_set_baud(ur->uart, (int)setup->b.wIndex.w*300);
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
      return -1;
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

  // enable USB
  usb_init();

  // default to silent mode to prevent issues with Ford
#ifdef PANDA_SAFETY
  controls_allowed = 0;
#else
  controls_allowed = 1;
#endif

  can_init(0);
  can_init(1);
  #ifdef CAN3
    can_init(2);
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
