#include "config.h"
#include "gpio.h"
#include "llgpio.h"
#include "early.h"
#include "can.h"
#include "rev.h"

void set_led(int led_num, int on) {
  if (led_num == -1) return;

  #ifdef PANDA
    set_gpio_output(GPIOC, led_num, !on);
  #else
    set_gpio_output(GPIOB, led_num, !on);
  #endif
}

/* In REVC, this should always be enabled under normal
 * operation. Otherwise, the CAN transceiver chip will try to pull the
 * pus lines to ground and anger other CAN devices. Eddie is looking
 * into if this is useful or not. */
void set_can_enable(uint8_t canid, int enabled) {
  // enable CAN busses
  gpio_pin *pin = &can_ports[canid].enable_pin;

  //enable,   high_val = 1;  1 xnor 1 = 1
  //enable,  ~high_val = 0;  1 xnor 0 = 0
  //~enable,  high_val = 0;  0 xnor 1 = 0
  //~enable, ~high_val = 1;  0 xnor 0 = 1
  set_gpio_output(pin->port, pin->num, !(enabled ^ pin->high_val));
}

// TODO: does this belong here?
void periph_init() {
  // enable GPIOB, UART2, CAN, USB clock
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
  #ifdef PANDA
    RCC->APB1ENR |= RCC_APB1ENR_UART5EN;
  #endif
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
  #ifdef CAN3
    RCC->APB1ENR |= RCC_APB1ENR_CAN3EN;
  #endif
  RCC->APB1ENR |= RCC_APB1ENR_DACEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
  //RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  // needed?
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
}

// board specific
void gpio_init() {
  // pull low to hold ESP in reset??
  // enable OTG out tied to ground
  GPIOA->ODR = 0;
  GPIOB->ODR = 0;
  GPIOA->PUPDR = 0;
  //GPIOC->ODR = 0;
  GPIOB->AFR[0] = 0;
  GPIOB->AFR[1] = 0;

  // C2,C3: analog mode, voltage and current sense
  set_gpio_mode(GPIOC, 2, MODE_ANALOG);
  set_gpio_mode(GPIOC, 3, MODE_ANALOG);

  // C8: FAN aka TIM3_CH4
  set_gpio_alternate(GPIOC, 8, GPIO_AF2_TIM3);

  // turn off LEDs and set mode
  set_led(LED_RED, 0);
  set_led(LED_GREEN, 0);
  set_led(LED_BLUE, 0);

  // A11,A12: USB
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG_FS);
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG_FS);
  GPIOA->OSPEEDR = GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;

#ifdef PANDA
  // enable started_alt on the panda
  set_gpio_pullup(GPIOA, 1, PULL_UP);

  // A2,A3: USART 2 for debugging
  set_gpio_alternate(GPIOA, 2, GPIO_AF7_USART2);
  set_gpio_alternate(GPIOA, 3, GPIO_AF7_USART2);

  // A9,A10: USART 1 for talking to the ESP
  set_gpio_alternate(GPIOA, 9, GPIO_AF7_USART1);
  set_gpio_alternate(GPIOA, 10, GPIO_AF7_USART1);

  // B12: GMLAN, ignition sense, pull up
  set_gpio_pullup(GPIOB, 12, PULL_UP);

  // A4,A5,A6,A7: setup SPI
  set_gpio_alternate(GPIOA, 4, GPIO_AF5_SPI1);
  set_gpio_alternate(GPIOA, 5, GPIO_AF5_SPI1);
  set_gpio_alternate(GPIOA, 6, GPIO_AF5_SPI1);
  set_gpio_alternate(GPIOA, 7, GPIO_AF5_SPI1);
#endif

  #ifdef PANDA
    // K-line enable moved from B4->B7 to make room for GMLAN on CAN3
    if(revision == REV_C)
      set_gpio_output(GPIOB, 7, 1); // REV C
    else
      set_gpio_output(GPIOB, 4, 1); // REV AB

    // C12,D2: K-Line setup on UART 5
    set_gpio_alternate(GPIOC, 12, GPIO_AF8_UART5);
    set_gpio_alternate(GPIOD, 2, GPIO_AF8_UART5);
    set_gpio_pullup(GPIOD, 2, PULL_UP);

    // L-line enable
    set_gpio_output(GPIOA, 14, 1);

    // C10,C11: L-Line setup on USART 3
    set_gpio_alternate(GPIOC, 10, GPIO_AF7_USART3);
    set_gpio_alternate(GPIOC, 11, GPIO_AF7_USART3);
    set_gpio_pullup(GPIOC, 11, PULL_UP);
  #endif

  if(revision == REV_C) {
    // B2,A13: set DCP mode on the charger (breaks USB!)
    //set_gpio_output(GPIOB, 2, 0);
    //set_gpio_output(GPIOA, 13, 0);

    //set_gpio_output(GPIOA, 13, 1); //CTRL 1
    //set_gpio_output(GPIOB, 2, 0);  //CTRL 2
  }
}
