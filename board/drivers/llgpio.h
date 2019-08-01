#define MODE_INPUT 0
#define MODE_OUTPUT 1
#define MODE_ALTERNATE 2
#define MODE_ANALOG 3

#define PULL_NONE 0
#define PULL_UP 1
#define PULL_DOWN 2

#define OUTPUT_TYPE_PUSH_PULL 0U
#define OUTPUT_TYPE_OPEN_DRAIN 1U

void set_gpio_mode(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  enter_critical_section();
  uint32_t tmp = GPIO->MODER;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
  GPIO->MODER = tmp;
  exit_critical_section();
}

void set_gpio_output(GPIO_TypeDef *GPIO, unsigned int pin, bool enabled) {
  enter_critical_section();
  if (enabled) {
    GPIO->ODR |= (1U << pin);
  } else {
    GPIO->ODR &= ~(1U << pin);
  }
  set_gpio_mode(GPIO, pin, MODE_OUTPUT);
  exit_critical_section();
}

void set_gpio_output_type(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int output_type){
  enter_critical_section();
  if(output_type == OUTPUT_TYPE_OPEN_DRAIN) {
    GPIO->OTYPER |= (1U << pin);
  } else {
    GPIO->OTYPER &= ~(1U << pin);
  }
  exit_critical_section();
}

void set_gpio_alternate(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  enter_critical_section();
  uint32_t tmp = GPIO->AFR[pin >> 3U];
  tmp &= ~(0xFU << ((pin & 7U) * 4U));
  tmp |= mode << ((pin & 7U) * 4U);
  GPIO->AFR[pin >> 3] = tmp;
  set_gpio_mode(GPIO, pin, MODE_ALTERNATE);
  exit_critical_section();
}

void set_gpio_pullup(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  enter_critical_section();
  uint32_t tmp = GPIO->PUPDR;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
  GPIO->PUPDR = tmp;
  exit_critical_section();
}

int get_gpio_input(GPIO_TypeDef *GPIO, unsigned int pin) {
  return (GPIO->IDR & (1U << pin)) == (1U << pin);
}

