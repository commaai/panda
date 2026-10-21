#pragma once

#include "board/crc.h"

// UM2331 CPU_SM_0, RAM_SM_0/7/8, FLASH_SM_0/7/9
// at boot: all of RAM in board/stm32h7/self_test.s before the C runtime, then CPU, FPU and the whole flash image
// at 8Hz: CPU, FPU, one RAM block and one flash chunk, ECC flags of every RAM in use

// keep in sync with board/stm32h7/self_test.s
#define SELF_TEST_PASS 0x5AFEC0DEU

// all IRQs share one priority, so each tick slice stays short
#define RAM_TEST_BLOCK_WORDS 64U
// consecutive blocks overlap by two words, for coupling across a block boundary
#define RAM_TEST_STEP ((RAM_TEST_BLOCK_WORDS - 2U) * 4U)
// see stm32h7x5_flash.ld
#define RAM_TEST_GUARD_SIZE 512U
// kept clear of the frames of the running test
#define RAM_TEST_STACK_WINDOW 512U
#define FLASH_CRC_CHUNK 0x400U

extern uint32_t _dtcmram_start;
extern uint32_t _dtcmram_end;
extern uint32_t _saxisram;
extern uint32_t _eaxisram;
extern uint32_t _sitcmram;
extern uint32_t _eitcmram;
extern uint32_t self_test_cookie;

typedef struct {
  uint32_t start;
  uint32_t end;
} ram_region;

// RM0468 Table 11, every RAM in use, not the ETM, FDCAN message and backup RAMs
static RAMECC_MonitorTypeDef *const ecc_monitors[] = {
  RAMECC1_Monitor1, RAMECC1_Monitor2, RAMECC1_Monitor3, RAMECC1_Monitor4, RAMECC1_Monitor6,
  RAMECC2_Monitor1, RAMECC2_Monitor2, RAMECC3_Monitor1,
};
#define ECC_MONITOR_CNT (sizeof(ecc_monitors) / sizeof(ecc_monitors[0]))

#ifdef ALLOW_DEBUG
#define SELF_TEST_CPU 0U
#define SELF_TEST_RAM 1U
#define SELF_TEST_FLASH 2U
static uint8_t self_test_fail = 0U;
static uint32_t flash_crc_flip = 0U;

// the flash injection goes through the CRC compare
void self_test_inject(uint8_t test) {
  if (test == SELF_TEST_FLASH) {
    flash_crc_flip = 1U;
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

// board/stm32h7/self_test.s
uint32_t cpu_test(void);
uint32_t fpu_test(void);

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

// transparent, interrupts stay off while the block holds test patterns
static bool ram_test_block(uint32_t addr, uint32_t words) {
  // outside the tested regions, copies are read back instead
  __attribute__((section(".sram12"))) static volatile uint32_t ram_test_backup[RAM_TEST_BLOCK_WORDS];
  volatile uint32_t *p = (uint32_t *)addr;
  bool ok = true;
  ENTER_CRITICAL()
  // the block can hold the MPU protected safety state
  mpu_safety_open();
  for (uint32_t i = 0U; i < words; i++) {
    ram_test_backup[i] = p[i];
  }
  for (uint32_t i = 0U; i < words; i++) {
    ok = ok && (ram_test_backup[i] == p[i]);
  }
  ok = ram_test_march(p, words, 0x00000000U) && ok;
  ok = ram_test_march(p, words, 0x55555555U) && ok;
  for (uint32_t i = 0U; i < words; i++) {
    p[i] = ram_test_backup[i];
  }
  for (uint32_t i = 0U; i < words; i++) {
    ok = ok && (p[i] == ram_test_backup[i]);
  }
  mpu_safety_close();
  EXIT_CRITICAL()
  return ok;
}

// the stack guard faults on access and the running frames can't hold test patterns, both are covered at boot
static bool ram_test_skip(uint32_t start, uint32_t end) {
  uint32_t guard = (uint32_t)&_stack_guard;
  uint32_t sp = __get_MSP();
  bool guard_hit = (start < (guard + RAM_TEST_GUARD_SIZE)) && (end > guard);
  bool stack_hit = (start < (sp + RAM_TEST_STACK_WINDOW)) && (end > (sp - RAM_TEST_STACK_WINDOW));
  return guard_hit || stack_hit;
}

// next block of the running pass, wraps around all regions
static bool ram_test_next(void) {
  // SRAM1/2 and SRAM4 hold the DMA buffers, which DMA can write mid test, and the test's own backup, they're covered at boot and by ECC
  static const ram_region ram_regions[] = {
    {(uint32_t)&_dtcmram_start, (uint32_t)&_dtcmram_end},
    {(uint32_t)&_saxisram, (uint32_t)&_eaxisram},
    {(uint32_t)&_sitcmram, (uint32_t)&_eitcmram},
  };
  static uint8_t region = 0U;
  static uint32_t block = 0U;
  if (block == 0U) {
    block = ram_regions[region].start;
  }
  uint32_t words = MIN(RAM_TEST_BLOCK_WORDS, (ram_regions[region].end - block) / 4U);
  bool ok = ram_test_skip(block, block + (words * 4U)) || ram_test_block(block, words);
  block += RAM_TEST_STEP;
  if (block >= ram_regions[region].end) {
    region = (uint8_t)((region + 1U) % (sizeof(ram_regions) / sizeof(ram_regions[0])));
    block = 0U;
  }
  return ok;
}

// board/crypto/sign.py: length, image, crc32 of everything before it, version tag and version, signature
static uint32_t flash_crc_len(void) {
  uint32_t len = (uint32_t)_app_start[0];
  return ((len >= 12U) && (len < sizeof(_app_start))) ? (len - 12U) : 0U;
}

static uint32_t flash_crc_stored(void) {
  const uint8_t *p = &((const uint8_t *)_app_start)[flash_crc_len()];
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static bool flash_test_next(void) {
  static uint32_t offset = 0U;
  static uint32_t crc = 0xFFFFFFFFU;
  bool ok = true;
  uint32_t len = flash_crc_len();
  uint32_t n = MIN(FLASH_CRC_CHUNK, len - offset);
  crc = crc32_update(crc, &((const uint8_t *)_app_start)[offset], n);
  offset += n;
  if (offset >= len) {
#ifdef ALLOW_DEBUG
    crc ^= flash_crc_flip;
    flash_crc_flip = 0U;
#endif
    ok = (len != 0U) && ((crc ^ 0xFFFFFFFFU) == flash_crc_stored());
    offset = 0U;
    crc = 0xFFFFFFFFU;
  }
  return ok;
}

static void self_test_check(bool cpu_ok, bool ram_ok, bool flash_ok) {
  bool cpu = cpu_ok;
  bool ram = ram_ok;
  bool flash = flash_ok;
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
  if (!flash) {
    fault_occurred(FAULT_SELF_TEST_FLASH);
  }
  // a CPU read with a double ECC error bus faults, this catches the DMA reads from flash
  if ((FLASH->SR1 & FLASH_SR_DBECCERR) != 0U) {
    fault_occurred(FAULT_ECC);
  }
  uint32_t ecc_sr = 0U;
  for (uint8_t i = 0U; i < ECC_MONITOR_CNT; i++) {
    ecc_sr |= ecc_monitors[i]->SR;
  }
  if ((ecc_sr & (RAMECC_SR_DEDF | RAMECC_SR_DEBWDF)) != 0U) {
    fault_occurred(FAULT_ECC);
  }
}

void self_test_init(void) {
  crc32_init();

  // the boot RAM test wrote every word, so no RAM holds an unset ECC code anymore
  for (uint8_t i = 0U; i < ECC_MONITOR_CNT; i++) {
    ecc_monitors[i]->SR = 0U;
    register_set(&(ecc_monitors[i]->CR), RAMECC_CR_ECCELEN, 0x3CU);
  }

  bool ram_ok = (self_test_cookie == SELF_TEST_PASS);
  self_test_cookie = 0U;
  uint32_t len = flash_crc_len();
  bool flash_ok = (len != 0U) && (crc32((const uint8_t *)_app_start, len) == flash_crc_stored());
  self_test_check((cpu_test() == SELF_TEST_PASS) && (fpu_test() == SELF_TEST_PASS), ram_ok, flash_ok);
}

// called at 8Hz
void self_test_tick(void) {
  bool cpu_ok = (cpu_test() == SELF_TEST_PASS) && (fpu_test() == SELF_TEST_PASS);
  bool ram_ok = ram_test_next();
  bool flash_ok = flash_test_next();
  self_test_check(cpu_ok, ram_ok, flash_ok);
}
