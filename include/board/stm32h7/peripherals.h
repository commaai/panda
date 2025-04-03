#pragma once

#ifdef BOOTSTUB
void gpio_usb_init(void);
#endif

void gpio_spi_init(void);
void gpio_usart2_init(void);
void gpio_uart7_init(void);
void common_init_gpio(void);
void flasher_peripherals_init(void);
void peripherals_init(void);
void enable_interrupt_timer(void);
