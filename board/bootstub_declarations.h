#pragma once
#include "platform_definitions.h"
#include "drivers/harness_configuration.h"
#include "boards/board.h"
// ******************** Prototypes ********************
void print(const char *a){ UNUSED(a); }
void puth(uint8_t i){ UNUSED(i); }
void puth2(uint8_t i){ UNUSED(i); }
void puth4(uint8_t i){ UNUSED(i); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }
void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
// No UART support in bootloader
typedef struct uart_ring {} uart_ring;
uart_ring uart_ring_som_debug;
uart_ring uart_ring_debug;
uart_ring *get_ring_by_number(int a);
void uart_init(uart_ring *q, int baud) { UNUSED(q); UNUSED(baud); }
bool put_char(uart_ring *q, char elem);

// ********************* Globals **********************
uint8_t hw_type = 0;
board *current_board;

// To make main_comms.h happy...
uint32_t uptime_cnt; // TODO: should this get a value?
bool ignition_can;
bool heartbeat_lost;
bool bootkick_reset_triggered;
