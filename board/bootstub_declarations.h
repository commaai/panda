#ifndef BOOTSTUB_DECLARATIONS_H
#define BOOTSTUB_DECLARATIONS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

#include "board/utils.h"

typedef struct board board;
typedef struct harness_configuration harness_configuration;

// ******************** Prototypes ********************
void print(const char *a);
void puth(unsigned int i);
void puth2(unsigned int i);
void hexdump(const void *a, int l);
typedef struct board board;
typedef struct harness_configuration harness_configuration;
void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
// No UART support in bootloader
typedef struct uart_ring uart_ring;
void uart_init(uart_ring *q, unsigned int baud);

// ********************* Globals **********************
extern uint8_t hw_type;
extern uint32_t enter_bootloader_mode;
extern const board *current_board;

#endif
