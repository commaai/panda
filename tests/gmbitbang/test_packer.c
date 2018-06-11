#include <stdio.h>
#include "../../board/drivers/canbitbang.h"

int main() {
  char out[100];
  int len = get_bit_message(out);
  printf("T:");
  for (int i = 0; i < len; i++) {
    printf("%d", out[i]);
  }
  printf("\n");
  printf("R:0000010010100000100010000010011110111010100111111111111111");
  printf("\n");
  return 0;
}



