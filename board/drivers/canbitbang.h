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

int append_bits(char *in, int in_len, char *app, int app_len) {
  for (int i = 0; i < app_len; i++) {
    in[in_len++] = app[i];
  }
  return in_len;
}

int append_int(char *in, int in_len, int val, int val_len) {
  for (int i = val_len-1; i >= 0; i--) {
    in[in_len++] = (val&(1<<i)) != 0;
  }
  return in_len;
}

int get_bit_message(char *out, CAN_FIFOMailBox_TypeDef *to_bang) {
  char pkt[MAX_BITS_CAN_PACKET];
  char footer[] = {
    1,  // CRC delimiter
    1,  // ACK
    1,  // ACK delimiter
    1,1,1,1,1,1,1, // EOF
    1,1,1, // IFS
  };
  #define SPEEED 30

  int len = 0;

  // test packet
  int dlc_len = to_bang->RDTR & 0xF;
  len = append_int(pkt, len, 0, 1);    // Start-of-frame
  len = append_int(pkt, len, to_bang->RIR >> 21, 11);  // Identifier
  len = append_int(pkt, len, 0, 3);    // RTR+IDE+reserved
  len = append_int(pkt, len, dlc_len, 4);    // Data length code

  // append data
  for (int i = 0; i < dlc_len; i++) {
    unsigned char dat = ((unsigned char *)(&(to_bang->RDLR)))[i];
    len = append_int(pkt, len, dat, 8);
  }

  // append crc
  len = append_crc(pkt, len);

  // do bitstuffing
  len = do_bitstuff(out, pkt, len);

  // append footer
  len = append_bits(out, len, footer, sizeof(footer));
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

  char pkt_stuffed[MAX_BITS_CAN_PACKET];
  int len = get_bit_message(pkt_stuffed, to_bang);

  // actual bitbang loop
  set_bitbanged_gmlan(1); // recessive
  set_gpio_mode(GPIOB, 13, MODE_OUTPUT);
  enter_critical_section();

  // wait for bus silent for 7 frames
  int silent_count = 0;
  while (silent_count < 7) {
    int read = get_gpio_input(GPIOB, 12);
    silent_count++;
    if (read == 0) {
      silent_count = 0;
    }
    int lwait = TIM2->CNT;
    while ((TIM2->CNT - lwait) < SPEEED);
  }

  // send my message with optional failure
  int last = 1;
  int init = TIM2->CNT;
  for (int i = 0; i < len; i++) {
    while ((TIM2->CNT - init) < (SPEEED*i));
    int read = get_gpio_input(GPIOB, 12);
    if ((read == 0 && last == 1) && i != (len-11)) {
      puts("ERR: bus driven at ");
      puth(i);
      puts("\n");
      goto fail;
    }
    set_bitbanged_gmlan(pkt_stuffed[i]);
    last = pkt_stuffed[i];
  }

fail:
  set_bitbanged_gmlan(1); // recessive
  exit_critical_section();
  set_gpio_mode(GPIOB, 13, MODE_INPUT);

  puts("bitbang done\n");
}

#endif

