#pragma once

#include <stdint.h>
#include <stdbool.h>

extern struct board board_tres;

void tres_set_can_mode(uint8_t mode);
bool tres_read_som_gpio (void);
