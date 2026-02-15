#pragma once

#define SOUND_RX_BUF_SIZE 1000U
#define SOUND_TX_BUF_SIZE (SOUND_RX_BUF_SIZE/2U)
#define MIC_RX_BUF_SIZE 512U
#define MIC_TX_BUF_SIZE (MIC_RX_BUF_SIZE * 2U)

extern uint16_t sound_rx_buf[2][SOUND_RX_BUF_SIZE];
extern uint16_t sound_tx_buf[2][SOUND_TX_BUF_SIZE];
extern uint32_t mic_rx_buf[2][MIC_RX_BUF_SIZE];
extern uint16_t mic_tx_buf[2][MIC_TX_BUF_SIZE];

#define SOUND_IDLE_TIMEOUT 4U
#define MIC_SKIP_BUFFERS 2U // Skip first 2 buffers (1024 samples = ~21ms at 48kHz)

void sound_tick(void);
void sound_init_dac(void);
void sound_stop_dac(void);
void sound_init(void);
