  .syntax unified
  .cpu cortex-m7
  .thumb

  .section .text.cpu_test_registers
  .global cpu_test_registers
  .type cpu_test_registers, %function

/* returns 0 if R0-R12, LR and APSR NZCVQ held both patterns */
cpu_test_registers:
  push {r4-r11, lr}
  .irp pattern, 0xAAAAAAAA, 0x55555555
  ldr r0, =\pattern
  mov r1, r0
  mov r2, r0
  mov r3, r0
  mov r4, r0
  mov r5, r0
  mov r6, r0
  mov r7, r0
  mov r8, r0
  mov r9, r0
  mov r10, r0
  mov r11, r0
  mov r12, r0
  mov lr, r0
  .irp reg, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr
  cmp \reg, r0
  bne 1f
  .endr
  /* r0 itself, against a fresh copy */
  ldr r1, =\pattern
  cmp r0, r1
  bne 1f
  .endr
  .irp flags, 0xA8000000, 0x50000000
  ldr r0, =\flags
  msr APSR_nzcvq, r0
  mrs r1, APSR
  and r1, r1, #0xF8000000
  cmp r1, r0
  bne 1f
  .endr
  movs r0, #0
  pop {r4-r11, pc}
1:
  movs r0, #1
  pop {r4-r11, pc}
  .ltorg
  .size cpu_test_registers, .-cpu_test_registers

  .fpu fpv5-d16
  .section .text.fpu_test_registers
  .global fpu_test_registers
  .type fpu_test_registers, %function

/* returns 0 if S0-S31 and FPSCR NZCV held both patterns */
fpu_test_registers:
  vpush {s16-s31}
  vmrs r3, fpscr
  .irp pattern, 0xAAAAAAAA, 0x55555555
  ldr r0, =\pattern
  .irp reg, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15, s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s30, s31
  vmov \reg, r0
  .endr
  .irp reg, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15, s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s30, s31
  vmov r1, \reg
  cmp r1, r0
  bne 2f
  .endr
  .endr
  .irp flags, 0xA0000000, 0x50000000
  ldr r0, =\flags
  vmsr fpscr, r0
  vmrs r1, fpscr
  and r1, r1, #0xF0000000
  cmp r1, r0
  bne 2f
  .endr
  vmsr fpscr, r3
  vpop {s16-s31}
  movs r0, #0
  bx lr
2:
  vmsr fpscr, r3
  vpop {s16-s31}
  movs r0, #1
  bx lr
  .ltorg
  .size fpu_test_registers, .-fpu_test_registers
