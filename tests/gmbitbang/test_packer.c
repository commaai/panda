#include <stdio.h>
#include "../../board/drivers/canbitbang.h"

int main() {
  char out[100];
  int len = get_bit_message(out);
  for (int i = 0; i < len; i++) {
    printf("%d", out[i]);
  }
  printf("\n");
  return 0;
}



