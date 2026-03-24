#pragma once
// CAN communication buffer handling
// Prototypes declared in board/comms.h and board/drivers/can_common.h
// Implementations in can_comms.c

void refresh_can_tx_slots_available(void);
void can_tx_comms_resume_usb(void);
void can_tx_comms_resume_spi(void);
