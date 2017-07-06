void set_can_enable(uint8_t canid, int enabled);

void set_led(int led_num, int on);

// TODO: does this belong here?
void periph_init();

// board specific
void gpio_init();
