#ifndef DRIVERS_USB_H
#define DRIVERS_USB_H

#include <stdint.h>
#include <stdbool.h>

void usb_irqhandler(void);
void can_tx_comms_resume_usb(void);

#endif
