//#define DEBUG
//#define CAN_LOOPBACK_MODE
//#define USE_INTERNAL_OSC

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

//#define PEDAL_USB

#ifdef PEDAL_USB
  #include "drivers/usb.h"
#endif

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeef
uint32_t enter_bootloader_mode;

void __initialize_hardware_early() {
  early();
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
/*
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
*/


// ***************************** toyota can checksum ****************************

int fix(uint8_t *dat, uint8_t len, uint16_t addr)
{
	uint8_t checksum = 0;
	checksum =((addr & 0xFF00) >> 8) + (addr & 0x00FF) + len +1;
	//uint16_t temp_msg = msg;
	
	for (int ii = 0; ii < 8; ii++)
	{
		checksum += (dat[ii]);
		//temp_msg = temp_msg >> 8;
	}
	
	//return ((msg & ~0xFF) & (checksum & 0xFF));
	return checksum;
}

// ***************************** can port *****************************

// addresses to be used on CAN
#define CAN_GAS_INPUT  0x200
#define CAN_GAS_OUTPUT 0x201
#define CAN_BRAKE_OUTPUT 0x343

void CAN1_TX_IRQHandler() {
  // clear interrupt
  CAN->TSR |= CAN_TSR_RQCP0;
}

// two independent values
uint16_t gas_set_0 = 0;
uint16_t gas_set_1 = 0;

#define MAX_TIMEOUT 10
uint32_t timeout = 0;
uint32_t current_index = 0;

#define NO_FAULT 0
#define FAULT_BAD_CHECKSUM 1
#define FAULT_SEND 2
#define FAULT_SCE 3
#define FAULT_STARTUP 4
#define FAULT_TIMEOUT 5
#define FAULT_INVALID 6
uint8_t state = FAULT_STARTUP;

void CAN1_RX0_IRQHandler() {
  while (CAN->RF0R & CAN_RF0R_FMP0) {
    #ifdef DEBUG
      puts("CAN RX\n");
    #endif
    uint32_t address = CAN->sFIFOMailBox[0].RIR>>21;
    if (address == CAN_GAS_INPUT) {
      // softloader entry
      if (CAN->sFIFOMailBox[0].RDLR == 0xdeadface) {
        if (CAN->sFIFOMailBox[0].RDHR == 0x0ab00b1e) {
          enter_bootloader_mode = ENTER_SOFTLOADER_MAGIC;
          NVIC_SystemReset();
        } else if (CAN->sFIFOMailBox[0].RDHR == 0x02b00b1e) {
          enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
          NVIC_SystemReset();
        }
      }

      // normal packet
	  //TODO: forward to 343 (for brakes) and send empty msgs with checksums
      uint8_t *dat = (uint8_t *)&CAN->sFIFOMailBox[0].RDLR;
      uint8_t *dat2 = (uint8_t *)&CAN->sFIFOMailBox[0].RDHR;
      int16_t accel_cmd = (dat[0] << 8) | dat[1];
      uint8_t set_me_x63 = (dat[2]);
      uint8_t release_standstill = (dat[3] & 0x80) >> 7;
	  uint8_t set_me_1 = (dat[3] & 0x40) >> 6;
	  uint8_t cancel_req = (dat[3] & 0xFF); // i guess? it's the first bit on the 4th message of 343
	  uint8_t cksum = dat2[3];
	  
	  //forward whole ACC message to 0x343
	  CAN->sTxMailBox[0].TIR = (CAN_BRAKE_OUTPUT << 21) | 1;
	  CAN->sTxMailBox[0].TDLR = dat[0] | (dat[1]<<8) | (dat[2]<<16) | (dat[3]<<24);
      CAN->sTxMailBox[0].TDHR = dat[4] | (dat[5]<<8) | (dat[6]<<16) | (dat[7]<<24);
      CAN->sTxMailBox[0].TDTR = 7;  // len of packet is 8
	  
	  //match calculated checksum to Eon's given checksum
      if (fix(*dat, 7, CAN_GAS_INPUT) == (dat2[7])) {
        if (set_me_1 == 1) {
          #ifdef DEBUG
            puts("setting gas ");
            puth(value);
            puts("\n");
            gas_set_0 = (pdl0 + accel_cmd); //default voltage is 1.6V
            gas_set_1 = (pdl1 + accel_cmd); //default voltage is 0.8V
          } else {
            if (accel_cmd == 0) {
              state = NO_FAULT;
            } else {
              state = FAULT_INVALID;
            }
            gas_set_0 = gas_set_1 = 0;
          }
          // clear the timeout
          timeout = 0;
        }
      } else {
        // wrong checksum = fault
        state = FAULT_BAD_CHECKSUM;
      }
    }
    // next
    CAN->RF0R |= CAN_RF0R_RFOM0;
  }
}

void CAN1_SCE_IRQHandler() {
  state = FAULT_SCE;
  can_sce(CAN);
}

int pdl0 = 0, pdl1 = 0;
int pkt_idx = 0;

int led_value = 0;

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
    uint8_t dat[8];
    dat[0] = (pdl0>>8)&0xFF;
    dat[1] = (pdl0>>0)&0xFF;
    dat[2] = (pdl1>>8)&0xFF;
    dat[3] = (pdl1>>0)&0xFF;
    dat[4] = state;
    dat[5] = fix(dat, 5, CAN_GAS_OUTPUT);
    CAN->sTxMailBox[0].TDLR = dat[0] | (dat[1]<<8) | (dat[2]<<16) | (dat[3]<<24);
    CAN->sTxMailBox[0].TDHR = dat[4] | (dat[5]<<8);
    CAN->sTxMailBox[0].TDTR = 6;  // len of packet is 5
    CAN->sTxMailBox[0].TIR = (CAN_GAS_OUTPUT << 21) | 1;
	
  } else {
    // old can packet hasn't sent!
    state = FAULT_SEND;
    #ifdef DEBUG
      puts("CAN MISS\n");
    #endif
  }

  // blink the LED
  set_led(LED_GREEN, led_value);
  led_value = !led_value;

  TIM3->SR = 0;

  // up timeout for gas set
  if (timeout == MAX_TIMEOUT) {
    state = FAULT_TIMEOUT;
  } else {
    timeout += 1;
  }
}

// ***************************** main code *****************************

void pedal() {
  // read/write
  pdl0 = adc_get(ADCCHAN_ACCEL0);
  pdl1 = adc_get(ADCCHAN_ACCEL1);

  // write the pedal to the DAC
  if (state == NO_FAULT) {
    dac_set(0, max(gas_set_0, pdl0));
    dac_set(1, max(gas_set_1, pdl1));
  } else {
    dac_set(0, pdl0);
    dac_set(1, pdl1);
  }

  // feed the watchdog
  IWDG->KR = 0xAAAA;
}

int main() {
  __disable_irq();

  // init devices
  clock_init();
  periph_init();
  gpio_init();

#ifdef PEDAL_USB
  // enable USB
  usb_init();
#endif

  // pedal stuff
  dac_init();
  adc_init();

  // init can
  can_silent = ALL_CAN_LIVE;
  can_init(0);

  // 48mhz / 65536 ~= 732
  timer_init(TIM3, 15);
  NVIC_EnableIRQ(TIM3_IRQn);

  // setup watchdog
  IWDG->KR = 0x5555;
  IWDG->PR = 0;          // divider /4
  // 0 = 0.125 ms, let's have a 50ms watchdog
  IWDG->RLR = 400 - 1;
  IWDG->KR = 0xCCCC;

  puts("**** INTERRUPTS ON ****\n");
  __enable_irq();

  // main pedal loop
  while (1) {
    pedal();
  }

  return 0;
}

