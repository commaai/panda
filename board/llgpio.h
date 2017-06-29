#ifndef PANDA_LLGPIO_H
#define PANDA_LLGPIO_H

#define MODE_INPUT 0
#define MODE_OUTPUT 1
#define MODE_ALTERNATE 2
#define MODE_ANALOG 3

#define PULL_NONE 0
#define PULL_UP 1
#define PULL_DOWN 2

void set_gpio_mode(GPIO_TypeDef *GPIO, int pin, int mode);

void set_gpio_output(GPIO_TypeDef *GPIO, int pin, int val);

void set_gpio_alternate(GPIO_TypeDef *GPIO, int pin, int mode);

void set_gpio_pullup(GPIO_TypeDef *GPIO, int pin, int mode);

int get_gpio_input(GPIO_TypeDef *GPIO, int pin);

#endif
