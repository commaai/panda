#include "config.h"
#include "early.h"
#include <stdbool.h>

// *** end config ***

#include "obj/gitversion.h"

// debug safety check: is controls allowed?
int started = 0;

// optional features
int started_signal_detected = 0;

// these are set in the Honda safety hooks...this is the wrong place
int controls_allowed = 0;
int gas_interceptor_detected = 0;

// Declare puts to supress warning
int puts(const char * str);

// ********************* includes *********************

#include "libc.h"
#include "gpio.h"
#include "uart.h"
#include "adc.h"
#include "timer.h"
#include "usb.h"
#include "safety.h"
#include "can.h"
#include "spi.h"

// ********************* serial debugging *********************

void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (getc(ring, &rcv)) {
    putc(ring, rcv);

    // jump to DFU flash
    if (rcv == 'z') {
      enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
      NVIC_SystemReset();
    }

    // normal reset
    if (rcv == 'x') {
      NVIC_SystemReset();
    }

    // enable CDP mode
    if (rcv == 'C') {
      puts("switching USB to CDP mode\n");
      set_usb_power_mode(USB_POWER_CDP);
    }
    if (rcv == 'c') {
      puts("switching USB to client mode\n");
      set_usb_power_mode(USB_POWER_CLIENT);
    }
    if (rcv == 'D') {
      puts("switching USB to DCP mode\n");
      set_usb_power_mode(USB_POWER_DCP);
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

  health->controls_allowed = controls_allowed;
  health->gas_interceptor_detected = gas_interceptor_detected;
  health->started_signal_detected = started_signal_detected;
  return sizeof(*health);
}

int usb_cb_ep1_in(uint8_t *usbdata, int len, int hardwired) {
  CAN_FIFOMailBox_TypeDef *reply = (CAN_FIFOMailBox_TypeDef *)usbdata;
  int ilen = 0;
  while (ilen < min(len/0x10, 4) && pop(&can_rx_q, &reply[ilen])) ilen++;
  return ilen*0x10;
}

// send on serial, first byte to select the ring
void usb_cb_ep2_out(uint8_t *usbdata, int len, int hardwired) {
  int i;
  if (len == 0) return;
  uart_ring *ur = get_ring_by_number(usbdata[0]);
  if (!ur) return;
  if ((usbdata[0] < 2) || safety_tx_lin_hook(usbdata[0]-2, usbdata+1, len-1)) {
    for (i = 1; i < len; i++) while (!putc(ur, usbdata[i]));
  }
}

// send on CAN
void usb_cb_ep3_out(uint8_t *usbdata, int len, int hardwired) {
  int dpkt = 0;
  for (dpkt = 0; dpkt < len; dpkt += 0x10) {
    uint32_t *tf = (uint32_t*)(&usbdata[dpkt]);

    // make a copy
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RDHR = tf[3];
    to_push.RDLR = tf[2];
    to_push.RDTR = tf[1];
    to_push.RIR = tf[0];

    uint8_t bus_number = (to_push.RDTR >> 4) & CAN_BUS_NUM_MASK;
    can_send(&to_push, bus_number);
  }
}

void usb_cb_enumeration_complete() {
  // power down the ESP
  // this doesn't work and makes the board unflashable
  // because the ESP spews shit on serial on startup
  //GPIOC->ODR &= ~(1 << 14);
  puts("USB enumeration complete\n");
}

int usb_cb_control_msg(USB_Setup_TypeDef *setup, uint8_t *resp, int hardwired) {
  int resp_len = 0;
  uart_ring *ur = NULL;
  int i;
  switch (setup->b.bRequest) {
    // **** 0xc0: get CAN debug info
    case 0xc0:
      puts("can tx: "); puth(can_tx_cnt);
      puts(" txd: "); puth(can_txd_cnt);
      puts(" rx: "); puth(can_rx_cnt);
      puts(" err: "); puth(can_err_cnt);
      puts("\n");
      break;
    // **** 0xd0: fetch serial number
    case 0xd0:
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
    // **** 0xd1: enter bootloader mode
    case 0xd1:
      if (hardwired) {
        enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
        NVIC_SystemReset();
      }
      break;
    // **** 0xd2: get health packet
    case 0xd2:
      resp_len = get_health_pkt(resp);
      break;
    // **** 0xd3: set fan speed
    case 0xd3:
      fan_set_speed(setup->b.wValue.w);
      break;
    // **** 0xd6: get version
    case 0xd6:
      COMPILE_TIME_ASSERT(sizeof(gitversion) <= MAX_RESP_LEN)
      memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = sizeof(gitversion);
      break;
    // **** 0xd8: reset ST
    case 0xd8:
      NVIC_SystemReset();
      break;
    // **** 0xd9: set ESP power
    case 0xd9:
      if (setup->b.wValue.w == 1) {
        // on
        GPIOC->ODR |= (1 << 14);
      } else {
        // off
        GPIOC->ODR &= ~(1 << 14);
      }
      break;
    // **** 0xda: reset ESP, with optional boot mode
    case 0xda:
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
    // **** 0xdb: set GMLAN multiplexing mode
    case 0xdb:
      #ifdef PANDA
        if (setup->b.wValue.w == 1) {
          // GMLAN ON
          if (setup->b.wIndex.w == 1) {
            can_set_gmlan(1);
          } else if (setup->b.wIndex.w == 2) {
            can_set_gmlan(2);
          }
        } else {
          can_set_gmlan(-1);
        }
      #endif
      break;
    // **** 0xdc: set safety mode
    case 0xdc:
      if (hardwired) {
        safety_set_mode(setup->b.wValue.w);
        can_set_silent(setup->b.wValue.w == SAFETY_NOOUTPUT);
      }
      break;
    // **** 0xdd: enable can forwarding
    case 0xdd:
      // wValue = Can Bus Num to forward from
      // wIndex = Can Bus Num to forward to
      if (setup->b.wValue.w < BUS_MAX && setup->b.wIndex.w < BUS_MAX &&
          setup->b.wValue.w != setup->b.wIndex.w) { // set forwarding
        can_forwarding[setup->b.wValue.w] = setup->b.wIndex.w & CAN_BUS_NUM_MASK;
      } else if(setup->b.wValue.w < BUS_MAX && setup->b.wIndex.w == 0xFF){ //Clear Forwarding
        can_forwarding[setup->b.wValue.w] = -1;
      }
      break;
    // **** 0xde: set can bitrate
    case 0xde:
      if (setup->b.wValue.w < BUS_MAX) {
        can_speed[setup->b.wValue.w] = setup->b.wIndex.w;
        can_init(CAN_NUM_FROM_BUS_NUM(setup->b.wValue.w));
      }
      break;
    // **** 0xe0: uart read
    case 0xe0:
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      // read
      while (resp_len < min(setup->b.wLength.w, MAX_RESP_LEN) && getc(ur, (char*)&resp[resp_len])) {
        ++resp_len;
      }
      break;
    // **** 0xe1: uart set baud rate
    case 0xe1:
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      uart_set_baud(ur->uart, setup->b.wIndex.w);
      break;
    // **** 0xe2: uart set parity
    case 0xe2:
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
    // **** 0xe4: uart set baud rate extended
    case 0xe4:
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      uart_set_baud(ur->uart, (int)setup->b.wIndex.w*300);
      break;
    // **** 0xe5: set CAN loopback (for testing)
    case 0xe5:
      can_loopback = (setup->b.wValue.w > 0);
      can_init_all();
      break;
    // **** 0xf0: do k-line wValue pulse on uart2 for Acura
    case 0xf0:
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

#ifdef ENABLE_SPI

void spi_cb_handle(uint8_t *data, int len) {
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

#endif

// ***************************** main code *****************************

void __initialize_hardware_early() {
  early();
}

int main() {
  // shouldn't have interrupts here, but just in case
  __disable_irq();

  // init devices
  clock_init();
  periph_init();

  // detect the revision and init the GPIOs
  detect();
  gpio_init();

  // enable main uart
  uart_init(USART2, 115200);

#ifdef PANDA
  // enable ESP uart
  uart_init(USART1, 115200);

  // enable LIN
  uart_init(UART5, 10400);
  UART5->CR2 |= USART_CR2_LINEN;
  uart_init(USART3, 10400);
  USART3->CR2 |= USART_CR2_LINEN;
#endif

  // enable USB
  usb_init();

  // default to silent mode to prevent issues with Ford
  safety_set_mode(SAFETY_NOOUTPUT);
  can_set_silent(1);
  can_init_all();

  adc_init();

#ifdef ENABLE_SPI
  spi_init();
#endif

  // set PWM
  fan_init();
  fan_set_speed(65535);

  puts("**** INTERRUPTS ON ****\n");

  __enable_irq();

  puts("OPTCR: "); puth(FLASH->OPTCR); puts("\n");

  // LED should keep on blinking all the time
  uint64_t cnt;
  for (cnt=0;;cnt++) {
    can_live = pending_can_live;

    // reset this every 16th pass
    if ((cnt&0xF) == 0) pending_can_live = 0;

    #ifdef DEBUG
      puts("** blink ");
      puth(can_rx_q.r_ptr); puts(" "); puth(can_rx_q.w_ptr); puts("  ");
      puth(can_tx1_q.r_ptr); puts(" "); puth(can_tx1_q.w_ptr); puts("  ");
      puth(can_tx2_q.r_ptr); puts(" "); puth(can_tx2_q.w_ptr); puts("\n");
    #endif

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
      fan_set_speed(32768);
    } else {
      started = 0;

      // turn off fan
      fan_set_speed(0);
    }
  }

  return 0;
}

