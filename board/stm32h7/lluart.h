void uart_init(uart_ring *q, int baud) { UNUSED(q); UNUSED(baud); }
void uart_set_baud(USART_TypeDef *u, unsigned int baud) { UNUSED(u); UNUSED(baud); }
void dma_pointer_handler(uart_ring *q, uint32_t dma_ndtr) { UNUSED(q); UNUSED(dma_ndtr); }
