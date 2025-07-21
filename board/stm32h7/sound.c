#include "sound.h"
#include "board/drivers/interrupts_declarations.h"
#include "board/faults_declarations.h"

#define SOUND_RX_BUF_SIZE 1000U
#define SOUND_TX_BUF_SIZE (SOUND_RX_BUF_SIZE/2U)
#define MIC_RX_BUF_SIZE 512U
#define MIC_TX_BUF_SIZE (MIC_RX_BUF_SIZE * 2U)
__attribute__((section(".sram4"))) static uint16_t sound_rx_buf[2][SOUND_RX_BUF_SIZE];
__attribute__((section(".sram4"))) static uint16_t sound_tx_buf[2][SOUND_TX_BUF_SIZE];
__attribute__((section(".sram4"))) static uint32_t mic_rx_buf[2][MIC_RX_BUF_SIZE];

#define SOUND_IDLE_TIMEOUT 4U
static uint8_t sound_idle_count;

// Implementation will be moved here - truncating for now to fix immediate build issue