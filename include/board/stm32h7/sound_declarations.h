#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SOUND_RX_BUF_SIZE 1000U
#define SOUND_TX_BUF_SIZE (SOUND_RX_BUF_SIZE/2U)
#define MIC_RX_BUF_SIZE 512U
#define MIC_TX_BUF_SIZE (MIC_RX_BUF_SIZE * 2U)

#define SOUND_IDLE_TIMEOUT 4U

void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t bits);
void register_clear_bits(volatile uint32_t *addr, uint32_t bits);

void sound_init(void);
void sound_tick(void);
