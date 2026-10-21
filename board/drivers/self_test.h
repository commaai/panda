#pragma once

#include "board/crc.h"

// UM2331 CPU_SM_0, RAM_SM_0/7/8, FLASH_SM_0/7

// all IRQs share one priority, so each tick slice stays short
#define RAM_TEST_BLOCK_WORDS 64U
#define FLASH_CRC_CHUNK 0x400U

extern uint32_t _sdata;
extern uint32_t _ebss;
extern uint32_t _saxisram;
extern uint32_t _eaxisram;
extern uint32_t _sitcmram;
extern uint32_t _eitcmram;
extern uint32_t _axisram_start;
extern uint32_t _axisram_end;
extern uint32_t _itcmram_start;
extern uint32_t _itcmram_end;

#define RAM_TEST_BLOCK_SIZE (RAM_TEST_BLOCK_WORDS * 4U)

typedef struct {
  uint32_t start;
  uint32_t end;
  bool preserve_at_init;
} ram_region;

// stack and DMA buffers (SRAM1/2, SRAM4) are left out
// .axisram and .itcmram are NOLOAD and unused before interrupts are on, so they get written first
static const ram_region ram_regions[] = {
  {(uint32_t)&_sdata, (uint32_t)&_ebss, true},
  {(uint32_t)&_saxisram, (uint32_t)&_eaxisram, false},
  {(uint32_t)&_sitcmram, (uint32_t)&_eitcmram, false},
};
#define RAM_REGION_CNT (sizeof(ram_regions) / sizeof(ram_regions[0]))

static uint32_t flash_crc_ref = 0U;

#ifdef ALLOW_DEBUG
#define SELF_TEST_CPU 0U
#define SELF_TEST_RAM 1U
#define SELF_TEST_FLASH 2U
static uint8_t self_test_fail = 0U;

void self_test_inject(uint8_t test) {
  if (test == SELF_TEST_FLASH) {
    flash_crc_ref ^= 1U;
  } else if (test <= SELF_TEST_RAM) {
    self_test_fail |= (uint8_t)(1U << test);
  } else {
  }
}

static bool self_test_injected(uint8_t test) {
  bool ret = (self_test_fail & (uint8_t)(1U << test)) != 0U;
  self_test_fail &= (uint8_t)~(1U << test);
  return ret;
}
#endif

// board/stm32h7/cpu_test.s
uint32_t cpu_test_registers(void);
uint32_t fpu_test_registers(void);

// march c-, word wide
static bool ram_test_march(volatile uint32_t *p, uint32_t n, uint32_t bg) {
  const uint32_t inv = ~bg;
  bool ok = true;
  for (uint32_t i = 0U; i < n; i++) {
    p[i] = bg;
  }
  for (uint32_t i = 0U; i < n; i++) {
    ok = ok && (p[i] == bg);
    p[i] = inv;
  }
  for (uint32_t i = 0U; i < n; i++) {
    ok = ok && (p[i] == inv);
    p[i] = bg;
  }
  for (uint32_t i = n; i > 0U; i--) {
    ok = ok && (p[i - 1U] == bg);
    p[i - 1U] = inv;
  }
  for (uint32_t i = n; i > 0U; i--) {
    ok = ok && (p[i - 1U] == inv);
    p[i - 1U] = bg;
  }
  for (uint32_t i = 0U; i < n; i++) {
    ok = ok && (p[i] == bg);
  }
  return ok;
}

// interrupts stay off while the block holds test patterns
static bool ram_test_block(uint32_t addr, uint32_t words, bool preserve) {
  // outside the tested regions, copies are read back instead
  __attribute__((section(".sram12"))) static volatile uint32_t ram_test_backup[RAM_TEST_BLOCK_WORDS];
  volatile uint32_t *p = (uint32_t *)addr;
  bool ok = true;
  ENTER_CRITICAL()
  // the block can hold the MPU protected safety state
  mpu_safety_open();
  if (preserve) {
    for (uint32_t i = 0U; i < words; i++) {
      ram_test_backup[i] = p[i];
    }
    for (uint32_t i = 0U; i < words; i++) {
      ok = ok && (ram_test_backup[i] == p[i]);
    }
  }
  ok = ram_test_march(p, words, 0x00000000U) && ok;
  ok = ram_test_march(p, words, 0x55555555U) && ok;
  if (preserve) {
    for (uint32_t i = 0U; i < words; i++) {
      p[i] = ram_test_backup[i];
    }
    for (uint32_t i = 0U; i < words; i++) {
      ok = ok && (p[i] == ram_test_backup[i]);
    }
  }
  mpu_safety_close();
  EXIT_CRITICAL()
  return ok;
}

static uint32_t ram_test_words(uint32_t block, uint32_t end) {
  return MIN(RAM_TEST_BLOCK_WORDS, (end - block) / 4U);
}

// next block of the running pass, wraps around all regions
static bool ram_test_next(void) {
  static uint8_t region = 0U;
  static uint32_t block = 0U;
  if (block == 0U) {
    block = ram_regions[region].start;
  }
  bool ok = ram_test_block(block, ram_test_words(block, ram_regions[region].end), true);
  block += RAM_TEST_BLOCK_SIZE;
  if (block >= ram_regions[region].end) {
    region = (uint8_t)((region + 1U) % RAM_REGION_CNT);
    block = 0U;
  }
  return ok;
}

static uint32_t flash_app_len(void) {
  uint32_t len = (uint32_t)_app_start[0];
  return (len < sizeof(_app_start)) ? len : 0U;
}

// crc over the app image, the bootstub checked its signature before jumping here
static uint32_t flash_crc(void) {
  return crc32((const uint8_t *)_app_start, flash_app_len());
}

static bool flash_test_next(void) {
  static uint32_t offset = 0U;
  static uint32_t crc = 0xFFFFFFFFU;
  bool ok = true;
  uint32_t len = flash_app_len();
  uint32_t n = MIN(FLASH_CRC_CHUNK, len - offset);
  crc = crc32_update(crc, &((const uint8_t *)_app_start)[offset], n);
  offset += n;
  if (offset >= len) {
    ok = (len != 0U) && ((crc ^ 0xFFFFFFFFU) == flash_crc_ref);
    offset = 0U;
    crc = 0xFFFFFFFFU;
  }
  return ok;
}

static void self_test_check(bool cpu_ok, bool ram_ok, bool flash_ok) {
  bool cpu = cpu_ok;
  bool ram = ram_ok;
#ifdef ALLOW_DEBUG
  cpu = cpu && !self_test_injected(SELF_TEST_CPU);
  ram = ram && !self_test_injected(SELF_TEST_RAM);
#endif
  if (!cpu) {
    fault_occurred(FAULT_SELF_TEST_CPU);
  }
  if (!ram) {
    fault_occurred(FAULT_SELF_TEST_RAM);
  }
  if (!flash_ok) {
    fault_occurred(FAULT_SELF_TEST_FLASH);
  }
  // double ECC errors also bus fault on the read, single ones are corrected
  if ((FLASH->SR1 & FLASH_SR_DBECCERR) != 0U) {
    fault_occurred(FAULT_ECC);
  }
  if (((RAMECC1_Monitor1->SR | RAMECC1_Monitor2->SR | RAMECC1_Monitor6->SR) & (RAMECC_SR_DEDF | RAMECC_SR_DEBWDF)) != 0U) {
    fault_occurred(FAULT_ECC);
  }
}

// a word never written since power-on can read as an ECC error, 64-bit stores since AXI SRAM and ITCM have 64-bit ECC words (RM0468 3.2)
static void ecc_ram_init(uint32_t start, uint32_t end) {
  for (uint32_t addr = start; addr < end; addr += 8U) {
    *((volatile uint64_t *)addr) = 0U;
  }
}

void self_test_init(void) {
  crc32_init();
  flash_crc_ref = flash_crc();
  ecc_ram_init((uint32_t)&_axisram_start, (uint32_t)&_axisram_end);
  ecc_ram_init((uint32_t)&_itcmram_start, (uint32_t)&_itcmram_end);

  bool ram_ok = true;
  for (uint8_t r = 0U; r < RAM_REGION_CNT; r++) {
    for (uint32_t block = ram_regions[r].start; block < ram_regions[r].end; block += RAM_TEST_BLOCK_SIZE) {
      ram_ok = ram_test_block(block, ram_test_words(block, ram_regions[r].end), ram_regions[r].preserve_at_init) && ram_ok;
    }
  }

  // RM0468 Table 11: RAMECC1 monitor 1 is AXI SRAM, 2 is ITCM, 6 is the AXI SRAM shared with ITCM, all fully written above (RM0468 Table 8, TCM_AXI_SHARED 00)
  // the other RAMs hold the stack and DMA buffers, which aren't written before their first read
  RAMECC1_Monitor1->SR = 0U;
  RAMECC1_Monitor2->SR = 0U;
  RAMECC1_Monitor6->SR = 0U;
  register_set(&(RAMECC1_Monitor1->CR), RAMECC_CR_ECCDEIE | RAMECC_CR_ECCELEN, 0x3CU);
  register_set(&(RAMECC1_Monitor2->CR), RAMECC_CR_ECCDEIE | RAMECC_CR_ECCELEN, 0x3CU);
  register_set(&(RAMECC1_Monitor6->CR), RAMECC_CR_ECCDEIE | RAMECC_CR_ECCELEN, 0x3CU);

  self_test_check((cpu_test_registers() == 0U) && (fpu_test_registers() == 0U), ram_ok, flash_app_len() != 0U);
}

// called at 8Hz
void self_test_tick(void) {
  bool cpu_ok = (cpu_test_registers() == 0U) && (fpu_test_registers() == 0U);
  bool ram_ok = ram_test_next();
  bool flash_ok = flash_test_next();
  self_test_check(cpu_ok, ram_ok, flash_ok);
}
