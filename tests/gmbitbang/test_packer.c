#include <stdio.h>
#include <stdint.h>

#define CANPACKET_DATA_SIZE_MAX 8
typedef struct __attribute__((packed)) {
  bool reserved2 : 1;
  bool returned : 1;
  bool extended : 1;  
  uint32_t addr : 29;
  uint32_t bus_time : 24;
  uint8_t bus : 2;
  uint8_t len : 6;
  uint8_t data[CANPACKET_DATA_SIZE_MAX];
} CANPacket_t;

#include "../../board/drivers/canbitbang.h"

int main() {
  char out[300];
  CANPacket_t to_bang = {0};
  to_bang.RIR = 20 << 21;
  to_bang.RDTR = 1;
  to_bang.RDLR = 1;

  int len = get_bit_message(out, &to_bang);
  printf("T:");
  for (int i = 0; i < len; i++) {
    printf("%d", out[i]);
  }
  printf("\n");
  printf("R:0000010010100000100010000010011110111010100111111111111111");
  printf("\n");
  return 0;
}



