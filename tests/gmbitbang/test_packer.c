#include <stdio.h>
#include <stdint.h>

#define CANPACKET_DATA_SIZE_MAX 8
typedef struct __attribute__((packed)) {
  unsigned char reserved : 1;
  unsigned char bus : 3;
  unsigned char data_len_code : 4;
  unsigned char rejected : 1;
  unsigned char returned : 1;
  unsigned char extended : 1;  
  unsigned int addr : 29;
  uint8_t data[CANPACKET_DATA_SIZE_MAX];
} CANPacket_t;

#include "../../board/drivers/canbitbang.h"

int main() {
  char out[300];
  CANPacket_t to_bang = {0};
  to_bang.addr = 20 << 18;
  to_bang.data_len_code = 1;
  to_bang.data[0] = 1;

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



