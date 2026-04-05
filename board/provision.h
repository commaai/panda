#ifndef PROVISION_H
#define PROVISION_H

#include "board/config.h"

#define PROVISION_CHUNK_LEN 0x20

void get_provision_chunk(uint8_t *resp);

#endif
