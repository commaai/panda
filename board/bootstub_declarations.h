// ******************** Prototypes ********************
void print(const char *a){ UNUSED(a); }
void puth(unsigned int i){ UNUSED(i); }
void puth2(unsigned int i){ UNUSED(i); }
__attribute__((unused)) static void puth4(unsigned int i){ UNUSED(i); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }
typedef struct board board;
void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
// No UART support in bootloader
typedef struct uart_ring uart_ring;
void uart_init(uart_ring *q, int baud) { UNUSED(q); UNUSED(baud); }

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;
