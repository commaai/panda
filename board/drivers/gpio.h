#define MODE_INPUT 0
#define MODE_OUTPUT 1
#define MODE_ALTERNATE 2
#define MODE_ANALOG 3

#define PULL_NONE 0
#define PULL_UP 1
#define PULL_DOWN 2

#define OUTPUT_TYPE_PUSH_PULL 0U
#define OUTPUT_TYPE_OPEN_DRAIN 1U

#define SPEED_LOW 0U
#define SPEED_MEDIUM 1U
#define SPEED_HIGH 2U
#define SPEED_VERY_HIGH 3U

void set_gpio_mode(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->MODER;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
  register_set(&(GPIO->MODER), tmp, 0xFFFFFFFFU);
  EXIT_CRITICAL();
}

void set_gpio_output(GPIO_TypeDef *GPIO, unsigned int pin, bool enabled) {
  ENTER_CRITICAL();
  if (enabled) {
    register_set_bits(&(GPIO->ODR), (1U << pin));
  } else {
    register_clear_bits(&(GPIO->ODR), (1U << pin));
  }
  set_gpio_mode(GPIO, pin, MODE_OUTPUT);
  EXIT_CRITICAL();
}

void set_gpio_output_type(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int output_type){
  ENTER_CRITICAL();
  if(output_type == OUTPUT_TYPE_OPEN_DRAIN) {
    register_set_bits(&(GPIO->OTYPER), (1U << pin));
  } else {
    register_clear_bits(&(GPIO->OTYPER), (1U << pin));
  }
  EXIT_CRITICAL();
}

void set_gpio_alternate(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->AFR[pin >> 3U];
  tmp &= ~(0xFU << ((pin & 7U) * 4U));
  tmp |= mode << ((pin & 7U) * 4U);
  register_set(&(GPIO->AFR[pin >> 3]), tmp, 0xFFFFFFFFU);
  set_gpio_mode(GPIO, pin, MODE_ALTERNATE);
  EXIT_CRITICAL();
}

void set_gpio_pullup(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->PUPDR;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (mode << (pin * 2U));
  register_set(&(GPIO->PUPDR), tmp, 0xFFFFFFFFU);
  EXIT_CRITICAL();
}

void set_gpio_speed(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int speed) {
  ENTER_CRITICAL();
  uint32_t tmp = GPIO->OSPEEDR;
  tmp &= ~(3U << (pin * 2U));
  tmp |= (speed << (pin * 2U));
  register_set(&(GPIO->OSPEEDR), tmp, 0xFFFFFFFFU);
  EXIT_CRITICAL();
}

int get_gpio_input(GPIO_TypeDef *GPIO, unsigned int pin) {
  return (GPIO->IDR & (1U << pin)) == (1U << pin);
}

// Detection with internal pullup
#define PULL_EFFECTIVE_DELAY 4096
bool detect_with_pull(GPIO_TypeDef *GPIO, int pin, int mode) {
  set_gpio_mode(GPIO, pin, MODE_INPUT);
  set_gpio_pullup(GPIO, pin, mode);
  for (volatile int i=0; i<PULL_EFFECTIVE_DELAY; i++);
  bool ret = get_gpio_input(GPIO, pin);
  set_gpio_pullup(GPIO, pin, PULL_NONE);
  return ret;
}
