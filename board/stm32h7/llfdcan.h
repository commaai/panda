#ifndef LLFDCAN_H
#define LLFDCAN_H

#include "board/drivers/fdcan.h"

#define SPEEDS_ARRAY_SIZE 8
#define DATA_SPEEDS_ARRAY_SIZE 10

extern const uint32_t speeds[SPEEDS_ARRAY_SIZE];
extern const uint32_t data_speeds[DATA_SPEEDS_ARRAY_SIZE];

bool fdcan_request_init(FDCAN_GlobalTypeDef *FDCANx);
bool fdcan_exit_init(FDCAN_GlobalTypeDef *FDCANx);
bool llcan_set_speed(FDCAN_GlobalTypeDef *FDCANx, uint32_t speed, uint32_t data_speed, bool non_iso, bool loopback, bool silent);
void llcan_irq_disable(const FDCAN_GlobalTypeDef *FDCANx);
void llcan_irq_enable(const FDCAN_GlobalTypeDef *FDCANx);
bool llcan_init(FDCAN_GlobalTypeDef *FDCANx);
void llcan_clear_send(FDCAN_GlobalTypeDef *FDCANx);

#endif
