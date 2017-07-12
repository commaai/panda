#define __DIV(_PCLK_, _BAUD_)                        (((_PCLK_)*25)/(4*(_BAUD_)))
#define __DIVMANT(_PCLK_, _BAUD_)                    (__DIV((_PCLK_), (_BAUD_))/100)
#define __DIVFRAQ(_PCLK_, _BAUD_)                    (((__DIV((_PCLK_), (_BAUD_)) - (__DIVMANT((_PCLK_), (_BAUD_)) * 100)) * 16 + 50) / 100)
#define __USART_BRR(_PCLK_, _BAUD_)              ((__DIVMANT((_PCLK_), (_BAUD_)) << 4)|(__DIVFRAQ((_PCLK_), (_BAUD_)) & 0x0F))

void uart_set_baud(USART_TypeDef *u, int baud) {
  if (u == USART1) {
    // USART1 is on APB2
    u->BRR = __USART_BRR(48000000, baud);
  } else {
    u->BRR = __USART_BRR(24000000, baud);
  }
}

void uart_init(USART_TypeDef *u, int baud) {
  // enable uart and tx+rx mode
  u->CR1 = USART_CR1_UE;
  uart_set_baud(u, baud);
  
  u->CR1 |= USART_CR1_TE | USART_CR1_RE;
  //u->CR2 = USART_CR2_STOP_0 | USART_CR2_STOP_1;
  //u->CR2 = USART_CR2_STOP_0;
  // ** UART is ready to work **

  // enable interrupts
  u->CR1 |= USART_CR1_RXNEIE;
}

void putch(const char a) {
  if (has_external_debug_serial) {
    putc(&debug_ring, a);
  } else {
    injectc(&debug_ring, a);
  }
}

int puts(const char *a) {
  for (;*a;a++) {
    if (*a == '\n') putch('\r');
    putch(*a);
  }
  return 0;
}

void puth(unsigned int i) {
  int pos;
  char c[] = "0123456789abcdef";
  for (pos = 28; pos != -4; pos -= 4) {
    putch(c[(i >> pos) & 0xF]);
  }
}

void puth2(unsigned int i) {
  int pos;
  char c[] = "0123456789abcdef";
  for (pos = 4; pos != -4; pos -= 4) {
    putch(c[(i >> pos) & 0xF]);
  }
}

void hexdump(void *a, int l) {
  int i;
  for (i=0;i<l;i++) {
    if (i != 0 && (i&0xf) == 0) puts("\n");
    puth2(((unsigned char*)a)[i]);
    puts(" ");
  }
  puts("\n");
}

