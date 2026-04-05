#include "board/config.h"
#include "board/utils.h"
#include "board/drivers/uart.h"

void uart_rx_ring(uart_ring *q){
  ENTER_CRITICAL();
  uint8_t c = ((USART_TypeDef *)q->uart)->RDR;
  uint16_t next_w_ptr = (q->w_ptr_rx + 1U) % q->rx_fifo_size;

  if ((next_w_ptr == q->r_ptr_rx) && q->overwrite) {
    q->r_ptr_rx = (q->r_ptr_rx + 1U) % q->rx_fifo_size;
  }

  if (next_w_ptr != q->r_ptr_rx) {
    q->elems_rx[q->w_ptr_rx] = c;
    q->w_ptr_rx = next_w_ptr;
    if (q->callback != NULL) {
      q->callback(q);
    }
  }
  EXIT_CRITICAL();
}

void uart_tx_ring(uart_ring *q){
  ENTER_CRITICAL();
  if (q->w_ptr_tx != q->r_ptr_tx) {
    if ((((USART_TypeDef *)q->uart)->ISR & USART_ISR_TXE_TXFNF) != 0U) {
      ((USART_TypeDef *)q->uart)->TDR = q->elems_tx[q->r_ptr_tx];
      q->r_ptr_tx = (q->r_ptr_tx + 1U) % q->tx_fifo_size;
    }

    if(q->r_ptr_tx != q->w_ptr_tx){
      ((USART_TypeDef *)q->uart)->CR1 |= USART_CR1_TXEIE;
    } else {
      ((USART_TypeDef *)q->uart)->CR1 &= ~USART_CR1_TXEIE;
    }
  }
  EXIT_CRITICAL();
}

#define UART_READ_RDR(uart) volatile uint8_t t = ((USART_TypeDef *)(uart))->RDR; UNUSED(t);

void uart_interrupt_handler(uart_ring *q) {
  ENTER_CRITICAL();
  uint32_t status = ((USART_TypeDef *)q->uart)->ISR;

  if((status & USART_ISR_RXNE_RXFNE) != 0U){
    uart_rx_ring(q);
  }

  uint32_t err = (status & USART_ISR_ORE) | (status & USART_ISR_NE) | (status & USART_ISR_FE) | (status & USART_ISR_PE);
  if(err != 0U){
    UART_READ_RDR(q->uart)
  }

  if ((err & USART_ISR_ORE) != 0U) {
    ((USART_TypeDef *)q->uart)->ICR |= USART_ICR_ORECF;
  } else if ((err & USART_ISR_NE) != 0U) {
    ((USART_TypeDef *)q->uart)->ICR |= USART_ICR_NECF;
  } else if ((err & USART_ISR_FE) != 0U) {
    ((USART_TypeDef *)q->uart)->ICR |= USART_ICR_FECF;
  } else if ((err & USART_ISR_PE) != 0U) {
    ((USART_TypeDef *)q->uart)->ICR |= USART_ICR_PECF;
  } else {}

  uart_tx_ring(q);
  EXIT_CRITICAL();
}

void UART7_IRQ_Handler(void) { uart_interrupt_handler(&uart_ring_som_debug); }

#ifndef BOOTSTUB
void uart_init(uart_ring *q, unsigned int baud) {
  if (q->uart == UART7) {
    REGISTER_INTERRUPT(UART7_IRQn, UART7_IRQ_Handler, 150000U, FAULT_INTERRUPT_RATE_UART_7)
    ((USART_TypeDef *)q->uart)->BRR = 60000000U / baud;
    ((USART_TypeDef *)q->uart)->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    ((USART_TypeDef *)q->uart)->CR1 |= USART_CR1_RXNEIE;
    NVIC_EnableIRQ(UART7_IRQn);
  }
}
#endif
