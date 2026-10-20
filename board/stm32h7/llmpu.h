#pragma once

// linker symbols, see stm32h7x5_flash.ld
extern uint32_t _ssafety;
extern uint32_t _stack_guard;

#define MPU_REGION_SAFETY 0U
#define MPU_REGION_STACK_GUARD 1U

static uint8_t mpu_safety_depth = 0U;

// normal memory, non-cacheable, never executed
static uint32_t mpu_rasr(uint32_t access, uint32_t size) {
  return MPU_RASR_XN_Msk | (access << MPU_RASR_AP_Pos) | (1UL << MPU_RASR_TEX_Pos) | (size << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;
}

static uint32_t mpu_safety_rasr(uint32_t access) {
  return mpu_rasr(access, ARM_MPU_REGION_SIZE_64B);
}

static uint32_t mpu_stack_guard_rasr(void) {
  return mpu_rasr(ARM_MPU_AP_NONE, ARM_MPU_REGION_SIZE_32B);
}

static void mpu_set_safety_access(uint32_t access) {
  MPU->RNR = MPU_REGION_SAFETY;
  MPU->RASR = mpu_safety_rasr(access);
  __DSB();
  __ISB();
}

// only safety code writes the safety state, interrupts stay off while it's writable
void mpu_safety_open(void) {
  ENTER_CRITICAL();
  if (mpu_safety_depth == 0U) {
    mpu_set_safety_access(ARM_MPU_AP_PRIV);
  }
  mpu_safety_depth += 1U;
}

void mpu_safety_close(void) {
  mpu_safety_depth -= 1U;
  if (mpu_safety_depth == 0U) {
    mpu_set_safety_access(ARM_MPU_AP_PRO);
  }
  EXIT_CRITICAL();
}

void mpu_init(void) {
  MPU->RNR = MPU_REGION_STACK_GUARD;
  MPU->RBAR = (uint32_t)&_stack_guard;
  MPU->RASR = mpu_stack_guard_rasr();

  MPU->RNR = MPU_REGION_SAFETY;
  MPU->RBAR = (uint32_t)&_ssafety;
  MPU->RASR = mpu_safety_rasr(ARM_MPU_AP_PRO);

  // privileged default map for everything else, a violation escalates to HardFault
  register_set(&(MPU->CTRL), MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk, 0x7U);
  __DSB();
  __ISB();
}

static bool mpu_region_ok(uint32_t region, uint32_t base, uint32_t rasr) {
  MPU->RNR = region;
  return ((MPU->RBAR & MPU_RBAR_ADDR_Msk) == base) && (MPU->RASR == rasr);
}

// RBAR and RASR are banked by RNR, so register_set can't read them back
void mpu_check(void) {
  ENTER_CRITICAL();
  bool ok = mpu_region_ok(MPU_REGION_STACK_GUARD, (uint32_t)&_stack_guard, mpu_stack_guard_rasr());
  ok = mpu_region_ok(MPU_REGION_SAFETY, (uint32_t)&_ssafety, mpu_safety_rasr(ARM_MPU_AP_PRO)) && ok;
  EXIT_CRITICAL();
  if (!ok) {
    fault_occurred(FAULT_REGISTER_DIVERGENT);
  }
}
