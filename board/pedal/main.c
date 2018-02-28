//#define DEBUG
//#define CAN_LOOPBACK_MODE
//#define USE_INTERNAL_OSC

#define PEDAL

#include "../config.h"

#include "drivers/drivers.h"

#include "drivers/llgpio.h"
#include "gpio.h"

#define CUSTOM_CAN_INTERRUPTS

#include "libc.h"
#include "safety.h"
#include "drivers/adc.h"
#include "drivers/uart.h"
#include "drivers/dac.h"
#include "drivers/can.h"
#include "drivers/timer.h"

#define CAN CAN1

#define PEDAL_USB

#ifdef PEDAL_USB
  #include "drivers/usb.h"
#endif

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeef
uint32_t enter_bootloader_mode;

void __initialize_hardware_early() {
  if (enter_bootloader_mode == ENTER_BOOTLOADER_MAGIC) {
    enter_bootloader_mode = 0;
    void (*bootloader)(void) = (void (*)(void)) (*((uint32_t *)0x1fff0004));
    bootloader();

    // LOOP
    while(1);
  }
}

// ********************* serial debugging *********************

void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (getc(ring, &rcv)) {
    putc(ring, rcv);
  }
}

#ifdef PEDAL_USB

int usb_cb_ep1_in(uint8_t *usbdata, int len, int hardwired) { return 0; }
void usb_cb_ep2_out(uint8_t *usbdata, int len, int hardwired) {}
void usb_cb_ep3_out(uint8_t *usbdata, int len, int hardwired) {}
void usb_cb_enumeration_complete() {}

int usb_cb_control_msg(USB_Setup_TypeDef *setup, uint8_t *resp, int hardwired) {
  int resp_len = 0;
  uart_ring *ur = NULL;
  switch (setup->b.bRequest) {
    // **** 0xe0: uart read
    case 0xe0:
      ur = get_ring_by_number(setup->b.wValue.w);
      if (!ur) break;
      if (ur == &esp_ring) uart_dma_drain();
      // read
      while ((resp_len < min(setup->b.wLength.w, MAX_RESP_LEN)) &&
                         getc(ur, (char*)&resp[resp_len])) {
        ++resp_len;
      }
      break;
  }
  return resp_len;
}

#endif

// ***************************** honda can checksum *****************************

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

// ***************************** can port *****************************

// addresses to be used on CAN
#define CAN_GAS_INPUT  0x200
#define CAN_GAS_OUTPUT 0x201

void CAN1_TX_IRQHandler() {
  // clear interrupt
  CAN->TSR |= CAN_TSR_RQCP0;
}

uint16_t gas_set = 0;
uint32_t timeout = 0;
uint32_t current_index = 0;

void CAN1_RX0_IRQHandler() {
  while (CAN->RF0R & CAN_RF0R_FMP0) {
    #ifdef DEBUG
      puts("CAN RX\n");
    #endif
    uint32_t address = CAN->sFIFOMailBox[0].RIR>>21;
    if (address == CAN_GAS_INPUT) {
      uint8_t *dat = (uint8_t *)&CAN->sFIFOMailBox[0].RDLR;
      uint16_t value = (dat[0] << 8) | dat[1];
      uint8_t index = (dat[2] >> 4) & 3;
      if (can_cksum(dat, 2, CAN_GAS_INPUT, index) == (dat[2] & 0xF)) {
        if (((current_index+1)&3) == index) {
          // TODO: set and start timeout 
          #ifdef DEBUG
            puts("setting gas ");
            puth(value);
            puts("\n");
          #endif
          gas_set = value;
          timeout = 0;
        }
        // TODO: better lockout? prevents same spam
        current_index = index;
      }
    }
    // next
    CAN->RF0R |= CAN_RF0R_RFOM0;
  }
}

void CAN1_SCE_IRQHandler() {
  can_sce(CAN);
}

int pdl0, pdl1;
int pkt_idx = 0;

void TIM3_IRQHandler() {
  #ifdef DEBUG
    puth(TIM3->CNT);
    puts(" ");
    puth(pdl0);
    puts(" ");
    puth(pdl1);
    puts("\n");
  #endif

  // check timer for sending the user pedal and clearing the CAN
  if ((CAN->TSR & CAN_TSR_TME0) == CAN_TSR_TME0) {
    uint8_t *dat = (uint8_t *)&CAN->sTxMailBox[0].TDLR;
    CAN->sTxMailBox[0].TDLR = (((pdl0>>8)&0xFF)<<0) |
                              (((pdl0>>0)&0xFF)<<8) |
                              (((pdl1>>8)&0xFF)<<16) |
                              (((pdl1>>0)&0xFF)<<24);
    CAN->sTxMailBox[0].TDHR = can_cksum(dat, 4, CAN_GAS_OUTPUT, pkt_idx) | (pkt_idx << 4);
    CAN->sTxMailBox[0].TDTR = 5;  // len of packet is 4
    CAN->sTxMailBox[0].TIR = (CAN_GAS_OUTPUT << 21) | 1;
    ++pkt_idx;
    pkt_idx &= 3;
  } else {
    // old can packet hasn't sent!
    // TODO: do something?
    #ifdef DEBUG
      puts("CAN MISS\n");
    #endif
  }


  // blink the other LED
  GPIOB->ODR |= (1 << 11);

  TIM3->SR = 0;

  // up timeout for gas set
  timeout++;
}

// ***************************** main code *****************************

void pedal() {
  // read/write
  pdl0 = adc_get(ADCCHAN_ACCEL0);
  pdl1 = adc_get(ADCCHAN_ACCEL1);

  // write the pedal to the DAC
  if (timeout < 10) {
    dac_set(0, max(gas_set, pdl0));
    dac_set(1, max(gas_set*2, pdl1));
  } else {
    dac_set(0, pdl0);
    dac_set(1, pdl1);
  }
}

int main() {
  // init devices
  clock_init();
  gpio_init();

  // pedal stuff
  dac_init();
  can_init(1);
  adc_init();

  // 48mhz / 65536 ~= 732
  timer_init(TIM3, 15);

  puts("**** INTERRUPTS ON ****\n");
  __disable_irq();

  NVIC_EnableIRQ(CAN1_TX_IRQn);
  NVIC_EnableIRQ(CAN1_RX0_IRQn);
  NVIC_EnableIRQ(CAN1_SCE_IRQn);

  NVIC_EnableIRQ(TIM3_IRQn);
  __enable_irq();

  // main pedal loop
  uint64_t cnt = 0;
  for (cnt=0;;cnt++) {
    pedal();
		set_led(LED_GREEN, (cnt&0xFFFF) < 0x8000);
  }

  return 0;
}

