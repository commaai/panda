/*
This driver configures the NXP UJA1023 chip on revB tesla giraffe
for output on startup and then allows the user to set the output
pins via set_uja1023_output_bits() and set_uja1023_output_buffer()

Set UJA1023 output pins. Bits = pins
P0 = bit 0
P1 = bit 1
...
P7 = bit 7
0b10101010 = P0 off, P1 on, P2 off, P3 on, P4 off, P5 on, P6 off, P7 on

Examples:
set bit 5:
set_uja1023_output_bits(1 << 5); //turn on any pins that = 1, leave all other pins alone

clear bit 5:
clear_uja1023_output_bits(1 << 5); //turn off any pins that = 1, leave all other pins alone

Or set the whole buffer at once:
set_uja1023_output_buffer(0xFF); //turn all output pins on
set_uja1023_output_buffer(0x00); //turn all output pins off
*/

#define LIN_RING lin2_ring

void uja1023_tx(LIN_FRAME_t *tx_frame) {
  clear_uart_buff(&LIN_RING);

  LIN_SendData(&LIN_RING, tx_frame);
  uart_flush(&LIN_RING);
  for (int i =0; i < 1+1+1+tx_frame->data_len+1; i++) {
    while (getc(&LIN_RING, NULL) == 0);
  }
}


int uja1023_rx(LIN_FRAME_t *rx_frame) {
  int ret = LIN_SendReceiveFrame(&LIN_RING, rx_frame);

  if (ret == 0) {
    puts("received LIN frame: ");
    for(int n=0; n < rx_frame->data_len; n++) {
      puth2(rx_frame->data[n]);
    }
    puts("\n");
  } else {
    puts("LIN error: ");
    puth2(ret);
    puts("\n");
  }
  return ret;
}

int uja1023_init(int addr) {
  int ret;
  LIN_FRAME_t frame_to_receive;
  frame_to_receive.data_len = 8;
  frame_to_receive.frame_id = 0x7D; //Response PID, 0x7D = 2 bit parity + 0x3C raw ID

  // bedtime
  /*LIN_FRAME_t sleep_frame;
  sleep_frame.data_len = 1;
  sleep_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  sleep_frame.data[0]  = 0;
  uja1023_tx(&sleep_frame);*/

  // pulse for wake-up
  /*GPIOC->MODER &= ~GPIO_MODER_MODER10_1;
  GPIOC->MODER |= GPIO_MODER_MODER10_0;
  GPIOC->ODR &= ~(1 << 10);
  for (int i = 0; i < 100; i++) {
    delay(8000);
    GPIOC->ODR |= (1 << 10);
    GPIOC->ODR &= ~(1 << 10);
  }
  GPIOC->MODER &= ~GPIO_MODER_MODER10_0;
  GPIOC->MODER |= GPIO_MODER_MODER10_1;
  delay(140 * 9000);*/

  // make frame for Assign frame ID
  LIN_FRAME_t assign_id_frame;
  assign_id_frame.data_len = 8;
  assign_id_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  assign_id_frame.data[0]  = addr; //D0, iniital node address, set to 0x60 default
  assign_id_frame.data[1]  = 0x06; //D1, protocol control info (PCI); should be 0x06
  assign_id_frame.data[2]  = 0xB1; //D2, service id (SID); should be 0xB4
  assign_id_frame.data[3]  = 0x11; //D3, supplier ID low byte; should be 0x11
  assign_id_frame.data[4]  = 0x00; //D4, supplier ID high byte; should be 0x00
  assign_id_frame.data[5]  = 0x00; //D5, function id low byte; should be 0x00
  assign_id_frame.data[6]  = 0x00; //D6, function id high byte; should be 0x00
  assign_id_frame.data[7]  = 0x04; //D7, slave node address (NAD). 0x04 means ID(PxReq) = 04, ID(PxResp) = 05
  uja1023_tx(&assign_id_frame);
  ret = uja1023_rx(&frame_to_receive);
  if (ret != LIN_OK) return 0;
  if (frame_to_receive.data[0] != addr) return 0;
  if (frame_to_receive.data[7] != 0xFF) return 0;

  // make frame for io_cfg_1; configure Px pins for push pull level output
  LIN_FRAME_t io_cfg_1_frame;
  io_cfg_1_frame.data_len = 8;
  io_cfg_1_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_1_frame.data[0]  = addr; //D0, slave node address (NAD) (default 0x60)
  io_cfg_1_frame.data[1]  = 0x06; //D1, protocol control info (PCI); should be 0x06
  io_cfg_1_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_1_frame.data[3]  = 0x00; //D3, bits 7-6 are 00 for first cfg block. Bits 5-0 are for IM/INH, RxDL, ADCIN cfg. D3 default is 0x00
  io_cfg_1_frame.data[4]  = 0x0c; //D4, High side enable (HSE). Set to 0xFF for High side driver or push pull
  io_cfg_1_frame.data[5]  = 0x01; //D4, Low side enable (LSE). Set to 0xFF for Low side driver or push pull
  io_cfg_1_frame.data[6]  = 0x00; //D6, Output mode (low byte) (OM0). Set to 0x00 for level output
  io_cfg_1_frame.data[7]  = 0x00; //D7, Output mode (high byte) (OM1). Set to 0x00 for level output
  uja1023_tx(&io_cfg_1_frame);
  ret = uja1023_rx(&frame_to_receive);
  if (ret != LIN_OK) return 0;
  if (frame_to_receive.data[0] != addr) return 0;
  if (frame_to_receive.data[7] != 0) return 0;

  // make frame for io_cfg_2; defaults
  LIN_FRAME_t io_cfg_2_frame;
  io_cfg_2_frame.data_len = 8;
  io_cfg_2_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_2_frame.data[0]  = addr; //D0, slave node address (NAD) (default 0x60)
  io_cfg_2_frame.data[1]  = 0x06; //D1, protocol control info (PCI); should be 0x06
  io_cfg_2_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_2_frame.data[3]  = 0x40; //D3, bits 7-6 are 01 for second cfg block. Bits 5-0 are for LSLP, TxDL, SMC, SMW, SM. D3 default is 0x40 (every bit 0 except for 7-6)
  io_cfg_2_frame.data[4]  = 0x00; //D4, Capture Mode (low byte) CM0. Set to 0x00 for no capture
  io_cfg_2_frame.data[5]  = 0x00; //D5, Capture Mode (high byte) CM1. Set to 0x00 for no capture
  io_cfg_2_frame.data[6]  = 0x00; //D6, Threshold select TH2 TH1. Default 0x00
  io_cfg_2_frame.data[7]  = 0x00; //D7, Local Wake up pin mask LWM. Default 0x00
  uja1023_tx(&io_cfg_2_frame);
  ret = uja1023_rx(&frame_to_receive);
  if (ret != LIN_OK) return 0;
  if (frame_to_receive.data[0] != addr) return 0;
  if (frame_to_receive.data[7] != 0) return 0;

  // make frame for io_cfg_3; set LH value, classic checksum, and LIN speeds up to 20kbps
  LIN_FRAME_t io_cfg_3_frame;
  io_cfg_3_frame.data_len = 8;
  io_cfg_3_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_3_frame.data[0]  = addr; //D0, slave node address (NAD) (default 0x60)
  io_cfg_3_frame.data[1]  = 0x04; //D1, protocol control info (PCI); should be 0x04
  io_cfg_3_frame.data[1]  = 0x02; //D1, protocol control info (PCI); should be 0x02
  io_cfg_3_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_3_frame.data[3]  = 0x80; //D3, bits 7-6 are 10 for third cfg block. Bits 5-0 are for LSC and ECC (classic vs enhanced checksum). D3 default is 0x80 (every bit 0 except for 7-6)
  io_cfg_3_frame.data[4]  = 0x00; //D4, Limp home mode output value. This is the value the output pins will go to if the bus times out or another error occurs
  io_cfg_3_frame.data[5]  = 0x10; //D5, PWM initial value; shouldn't matter but is 0x10 per datasheet example
  io_cfg_3_frame.data[3]  = 0xC0; //D3, bits 7-6 are 11 for fourth cfg block. Bits 5-0 are unused and should be set to 0; should be 0xC0
  io_cfg_3_frame.data[4]  = 0xFF; //D4, Not used, set to 0xFF
  io_cfg_3_frame.data[5]  = 0xFF; //D5, Not used, set to 0xFF
  io_cfg_3_frame.data[6]  = 0xFF; //D6, Not used, set to 0xFF
  io_cfg_3_frame.data[7]  = 0xFF; //D7, Not used, set to 0xFF
  uja1023_tx(&io_cfg_3_frame);
  ret = uja1023_rx(&frame_to_receive);
  if (ret != LIN_OK) return 0;
  if (frame_to_receive.data[0] != addr) return 0;
  if (frame_to_receive.data[7] != 0xff) return 0;

  return 1;
}

// set the whole Px output buffer at once:
void set_uja1023_output_buffer(uint8_t to_set) {
  // make frame for PxReq frame (this is what sets the UJA1023's output pins)
  LIN_FRAME_t px_req_frame;
  px_req_frame.data_len = 2;
  px_req_frame.frame_id = 0xC4; //PID, 0xC4 = 2 bit parity + 0x04 raw ID
  px_req_frame.data[0] = to_set;
  px_req_frame.data[1]  = 0x80; //D1, PWM Value; shouldn't matter but is 0x80 per datasheet example
  LIN_SendData(&LIN_RING, &px_req_frame);
}


