#include "config.h"
#include "early.h"
#include "obj/gitversion.h"
#include "uart.h"
#include "can.h"
#include "libc.h"
#include "gpio.h"
#include "adc.h"
#include "timer.h"
#include "usb.h"
#include "spi.h"
#include "safety.h"

int started = 0;

// optional features
int started_signal_detected = 0;

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

  health->controls_allowed = is_output_enabled();

  health->gas_interceptor_detected = gas_interceptor_detected;
  health->started_signal_detected = started_signal_detected;
  return sizeof(*health);
}

void set_fan_speed(int fan_speed) {
  TIM3->CCR3 = fan_speed;
}

void usb_cb_ep0_out(USB_Setup_TypeDef *setup, uint8_t *usbdata, int hardwired) {
  if (setup->b.bRequest == 0xde) {
    puts("Setting baud rate from usb\n");
    uint32_t bitrate = *(int*)usbdata;
    uint16_t canb_id = setup->b.wValue.w;

    if (can_ports[canb_id].gmlan)
      can_ports[canb_id].gmlan_bitrate = bitrate;
    else
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

    int canid = (to_push.RDTR >> 4) & 0xF;
    if (safety_tx_hook(&to_push, hardwired)) {
      send_can(&to_push, canid);
    }
  }
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
      puts("Toggle GMLAN canid: ");
      puth(setup->b.wValue.w);

      uint16_t canid = setup->b.wValue.w;
      bool gmlan_enable = setup->b.wIndex.w;

      puth(canid);
      puts(" mode ");
      puth(gmlan_enable);
      puts("\n");

      if (canid >= CAN_MAX) {
        puts(" !!Out of range!!\n");
        return -1;
      }

      can_port_desc *port = &can_ports[canid];

      //puts(" gmlan_support ");
      //puth(port->gmlan_support);
      //puts(" mode ");
      //puth(gmlan_enable);
      //puts("\n");

      //Fail if canid doesn't support gmlan
      if (!port->gmlan_support) {
        puts(" !!Unsupported!!\n");
        return -1;
      }

      //ACK the USB pipe but don't do anything; nothing to do.
      if (port->gmlan == gmlan_enable) {
        puts(" ~~Nochange~~.\n");
        break;
      }

      // Check to see if anyther canid is acting as gmlan, disable it.
      set_can_mode(canid, gmlan_enable);
      puts(" Done\n");
      break;
    case 0xdc: // set controls allowed / safety policy
      set_safety_mode(setup->b.wValue.w);
      for(i=0; i < CAN_MAX; i++)
        can_init(i);
      break;
    case 0xdd: // enable can forwarding
      if (setup->b.wValue.w < CAN_MAX) { //Set forwarding
        can_ports[setup->b.wValue.w].forwarding = setup->b.wIndex.w;
      } else if (setup->b.wValue.w == 0xFF) { //Clear Forwarding
        can_ports[setup->b.wValue.w].forwarding = -1;
      } else
        return -1;
      break;
    case 0xde: // Set Can bitrate
      if (!(setup->b.wValue.w < CAN_MAX && setup->b.wLength.w == 4))
        return -1;
      break;
    case 0xdf: // Get Can bitrate
      if (setup->b.wValue.w < CAN_MAX) {
        if(can_ports[setup->b.wValue.w].gmlan){
          memcpy(resp, (void *)&can_ports[setup->b.wValue.w].gmlan_bitrate, 4);
        }else{
          memcpy(resp, (void *)&can_ports[setup->b.wValue.w].bitrate, 4);
        }
        resp_len = 4;
        break;
      }
      puts("Invalid num\n");
      return -1;
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
    case 0xe5: // Set CAN loopback (for testing)
      can_loopback = (setup->b.wValue.w > 0);
      for(i=0; i < CAN_MAX; i++)
        can_init(i);
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

//TODO Jessy: Audit for overflows
void handle_spi(uint8_t *data, int len) {
  USB_Setup_TypeDef *fake_setup;
  memset(spi_tx_buf, 0xaa, 0x44);
  // data[0]  = endpoint
  // data[2]  = length
  // data[4:] = data
  int *resp_len = (int*)spi_tx_buf;
  *resp_len = 0;
  switch (data[0]) {
    case 0:
      // control transfer
      fake_setup = (USB_Setup_TypeDef *)(data+4);
      *resp_len = usb_cb_control_msg(fake_setup, spi_tx_buf+4, 0);
      // Handle CTRL writes with data
      if (*resp_len == 0 && (fake_setup->b.bmRequestType & 0x80) == 0 && fake_setup->b.wLength.w)
        usb_cb_ep0_out(fake_setup, data+4+sizeof(USB_Setup_TypeDef), 0);
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
  int i;

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

  for(i=0; i < CAN_MAX; i++)
    can_init(i);

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

    // set LED to be controls allowed, blue on panda, green on legacy
    #ifdef PANDA
      set_led(LED_BLUE, is_output_enabled());
    #else
      set_led(LED_GREEN, is_output_enabled());
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
