#pragma once

void gpio_usb_init(void);
void gpio_spi_init(void);

#ifdef BOOTSTUB
void gpio_usart2_init(void);
#endif

void gpio_uart7_init(void);

// Common GPIO initialization
void common_init_gpio(void);

#ifdef BOOTSTUB
void flasher_peripherals_init(void);
#endif

// Peripheral initialization
void peripherals_init(void);

void enable_interrupt_timer(void);