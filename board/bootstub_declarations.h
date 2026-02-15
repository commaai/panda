#pragma once

// ******************** Prototypes ********************
void print(const char *a);
void puth(uint8_t i);
void puth2(uint8_t i);
void puth4(uint8_t i);
void hexdump(const void *a, int l);
typedef struct board board;
typedef struct harness_configuration harness_configuration;
void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
// No UART support in bootloader
typedef struct uart_ring {} uart_ring;
extern uart_ring uart_ring_som_debug;
void uart_init(uart_ring *q, int baud);

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;
