#pragma once
#include <stdbool.h>
#include "platform_definitions.h"

#define MODE_INPUT 0
#define MODE_OUTPUT 1
#define MODE_ALTERNATE 2
#define MODE_ANALOG 3

#define PULL_NONE 0
#define PULL_UP 1
#define PULL_DOWN 2

#define OUTPUT_TYPE_PUSH_PULL 0U
#define OUTPUT_TYPE_OPEN_DRAIN 1U

typedef struct {
    GPIO_TypeDef * const bank;
    uint8_t pin;
} gpio_t;

void set_gpio_mode(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode);
void set_gpio_output(GPIO_TypeDef *GPIO, unsigned int pin, bool enabled);
void set_gpio_output_type(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int output_type);
void set_gpio_alternate(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode);
void set_gpio_pullup(GPIO_TypeDef *GPIO, unsigned int pin, unsigned int mode);
int get_gpio_input(const GPIO_TypeDef *GPIO, unsigned int pin);
void gpio_set_all_output(gpio_t *pins, uint8_t num_pins, bool enabled);
void gpio_set_bitmask(gpio_t *pins, uint8_t num_pins, uint32_t bitmask);
bool detect_with_pull(GPIO_TypeDef *GPIO, int pin, int mode);
