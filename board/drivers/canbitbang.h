#define MAX_BITS_CAN_PACKET (64+44+25)

// returns out_len
int do_bitstuff(char *out, char *in, int in_len) {
  int last_bit = -1;
  int bit_cnt = 0;
  int j = 0;
  for (int i = 0; i < in_len; i++) {
    char bit = in[i];
    out[j++] = bit;

    // do the stuffing
    if (bit == last_bit) {
      bit_cnt++;
      if (bit_cnt == 5) {
        // 5 in a row the same, do stuff
        last_bit = !bit;
        out[j++] = last_bit;
        bit_cnt = 1;
      }
    } else {
      // this is a new bit
      last_bit = bit;
      bit_cnt = 1;
    }
  }
  return j;
}

int append_crc(char *in, int in_len) {
  int crc = 0;
  for (int i = 0; i < in_len; i++) {
    crc <<= 1;
    if (in[i] ^ ((crc>>15)&1)) {
      crc = crc ^ 0x4599;
    }
    crc &= 0x7fff;
  }
  for (int i = 14; i >= 0; i--) {
    in[in_len++] = (crc>>i)&1;
  }
  return in_len;
}

int get_bit_message(char *out) {
  char test_pkt[MAX_BITS_CAN_PACKET];
  char test_pkt_src[] = {
    0, // SOF
    0,0,0,0, // ID10-ID7
    0,0,1,0,1,0,0,   // ID6-ID0
    0, // RTR
    0, // IDE
    0, // reserved
    0,0,0,1, // len
    0,0,0,0,0, // 1st byte 7-3
    0,0,1,     // 1st byte 2-0
  };

  char footer[] = {
    1,  // CRC delimiter

    1,  // ACK
    1,  // ACK delimiter
    1,1,1,1,1,1,1, // EOF
    1,1,1, // IFS
  };
  #define SPEEED 30

  // copy packet
  for (int i = 0; i < sizeof(test_pkt_src); i++) test_pkt[i] = test_pkt_src[i];

  // append crc
  int len = append_crc(test_pkt, sizeof(test_pkt_src));

  // do bitstuffing
  len = do_bitstuff(out, test_pkt, len);

  // append footer
  for (int i = 0; i < sizeof(footer); i++) {
    out[len++] = footer[i];
  }
  return len;
}

// hardware stuff below this line

#ifdef PANDA

void set_bitbanged_gmlan(int val) {
  if (val) {
    GPIOB->ODR |= (1 << 13);
  } else {
    GPIOB->ODR &= ~(1 << 13);
  }
}

void bitbang_gmlan(CAN_FIFOMailBox_TypeDef *to_bang) {
  puts("called bitbang_gmlan\n");
  puth(can_num_lookup[3]);
  puts("\n");

  char pkt_stuffed[MAX_BITS_CAN_PACKET];
  int len = get_bit_message(pkt_stuffed);

  // actual bitbang loop
  set_bitbanged_gmlan(1); // recessive
  set_gpio_mode(GPIOB, 13, MODE_OUTPUT);
  enter_critical_section();
  int init = TIM2->CNT;

  for (int i = 0; i < len; i++) {
    while ((TIM2->CNT - init) < (SPEEED*i));
    set_bitbanged_gmlan(pkt_stuffed[i]);
  }

  exit_critical_section();
  set_gpio_mode(GPIOB, 13, MODE_INPUT);

  puts("bitbang done\n");
}

#endif

