void set_can_enable(CAN_TypeDef *CAN, int enabled);

void set_led(int led_num, int on);

// TODO: does this belong here?
void periph_init();

void set_can_mode(int can, int use_gmlan);

// board specific
void gpio_init();
