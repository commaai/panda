#ifndef PANDA_UART_H
#define PANDA_UART_H

#define __DIV(_PCLK_, _BAUD_)       (((_PCLK_)*25)/(4*(_BAUD_)))
#define __DIVMANT(_PCLK_, _BAUD_)   (__DIV((_PCLK_), (_BAUD_))/100)
#define __DIVFRAQ(_PCLK_, _BAUD_)   (((__DIV((_PCLK_), (_BAUD_)) - (__DIVMANT((_PCLK_), (_BAUD_)) * 100)) * 16 + 50) / 100)
#define __USART_BRR(_PCLK_, _BAUD_) ((__DIVMANT((_PCLK_), (_BAUD_)) << 4)|(__DIVFRAQ((_PCLK_), (_BAUD_)) & 0x0F))

typedef struct uart_ring {
  uint8_t w_ptr_tx;
  uint8_t r_ptr_tx;
  uint8_t elems_tx[FIFO_SIZE];
  uint8_t w_ptr_rx;
  uint8_t r_ptr_rx;
  uint8_t elems_rx[FIFO_SIZE];
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
} uart_ring;

extern int has_external_debug_serial;

extern uart_ring debug_ring;

void debug_ring_callback(uart_ring *ring);

void uart_set_baud(USART_TypeDef *u, int baud);

void uart_init(USART_TypeDef *u, int baud);

void putch(const char a);

int puts(const char *a);

void puth(unsigned int i);

void puth2(unsigned int i);

void hexdump(void *a, int l);

int getc(uart_ring *q, char *elem);

int putc(uart_ring *q, char elem);

int injectc(uart_ring *q, char elem);

void uart_ring_process(uart_ring *q);

#endif
