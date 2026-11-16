/**
  ******************************************************************************
  * @file      startup_stm32h735xx.s
  * @author    MCD Application Team
  * @brief     STM32H735xx Devices vector table for GCC based toolchain.
  *            This module performs:
  *                - Set the initial SP
  *                - Set the initial PC == Reset_Handler,
  *                - Set the vector table entries with the exceptions ISR address
  *                - Branches to main in the C library (which eventually
  *                  calls main()).
  *            After Reset the Cortex-M processor is in Thread mode,
  *            priority is Privileged, and the Stack is set to Main.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

  .syntax unified
  .cpu cortex-m7
  .fpu softvfp
  .thumb

.global  g_pfnVectors
.global  Default_Handler

/* start address for the initialization values of the .data section.
defined in linker script */
.word  _sidata
/* start address for the .data section. defined in linker script */
.word  _sdata
/* end address for the .data section. defined in linker script */
.word  _edata
/* start address for the .bss section. defined in linker script */
.word  _sbss
/* end address for the .bss section. defined in linker script */
.word  _ebss
/* stack used for SystemInit_ExtMemCtl; always internal RAM used */

/**
 * @brief  This is the code that gets called when the processor first
 *          starts execution following a reset event. Only the absolutely
 *          necessary set is performed, after which the application
 *          supplied main() routine is called.
 * @param  None
 * @retval : None
*/

    .section  .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function
Reset_Handler:
  ldr   sp, =_estack      /* set stack pointer */
  bl initialize_hardware_early
  bl ram_test_startup

/* Copy the data segment initializers from flash to SRAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit
/* Zero fill the bss segment. */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss
  
/* Call the clock system intitialization function.*/
  /* bl  SystemInit    */
/* Call static constructors */
    /* bl __libc_init_array */
/* Call the application's entry point.*/
  bl  main
  bx  lr
.size  Reset_Handler, .-Reset_Handler

/**
 * @brief  This is the code that gets called when the processor receives an
 *         unexpected interrupt.  It resets through the HardFault handler, so the
 *         reset shows up in health.
 * @param  None
 * @retval None
*/
/* the app and body firmware link the real one from self_test.s, the bootstub doesn't test RAM */
    .section  .text.ram_test_none,"ax",%progbits
ram_test_none:
  bx lr
  .size  ram_test_none, .-ram_test_none
  .weak  ram_test_startup
  .thumb_set ram_test_startup, ram_test_none

    .section  .text.Default_Handler,"ax",%progbits
Default_Handler:
  b  HardFault_Handler
  .size  Default_Handler, .-Default_Handler
/******************************************************************************
*
* The minimal vector table for a Cortex M. Note that the proper constructs
* must be placed on this to ensure that it ends up at physical address
* 0x0000.0000.
*
*******************************************************************************/
   .section  .isr_vector,"a",%progbits
  .type  g_pfnVectors, %object
  .size  g_pfnVectors, .-g_pfnVectors


g_pfnVectors:
  .word  _estack
  .word  Reset_Handler

  .word  NMI_Handler
  .word  HardFault_Handler
  .word  MemManage_Handler
  .word  BusFault_Handler
  .word  UsageFault_Handler
  .word  0
  .word  0
  .word  0
  .word  0
  .word  SVC_Handler
  .word  DebugMon_Handler
  .word  0
  .word  PendSV_Handler
  .word  SysTick_Handler

  /* External Interrupts, handle_interrupt in board/drivers/interrupts.h dispatches on the IRQ number in IPSR */
  .word     handle_interrupt                  /* Window WatchDog              */
  .word     handle_interrupt                  /* PVD/AVD through EXTI Line detection */
  .word     handle_interrupt                  /* Tamper and TimeStamps through the EXTI line */
  .word     handle_interrupt                  /* RTC Wakeup through the EXTI line */
  .word     handle_interrupt                  /* FLASH                        */
  .word     handle_interrupt                  /* RCC                          */
  .word     handle_interrupt                  /* EXTI Line0                   */
  .word     handle_interrupt                  /* EXTI Line1                   */
  .word     handle_interrupt                  /* EXTI Line2                   */
  .word     handle_interrupt                  /* EXTI Line3                   */
  .word     handle_interrupt                  /* EXTI Line4                   */
  .word     handle_interrupt                  /* DMA1 Stream 0                */
  .word     handle_interrupt                  /* DMA1 Stream 1                */
  .word     handle_interrupt                  /* DMA1 Stream 2                */
  .word     handle_interrupt                  /* DMA1 Stream 3                */
  .word     handle_interrupt                  /* DMA1 Stream 4                */
  .word     handle_interrupt                  /* DMA1 Stream 5                */
  .word     handle_interrupt                  /* DMA1 Stream 6                */
  .word     handle_interrupt                  /* ADC1, ADC2 and ADC3s         */
  .word     handle_interrupt                  /* FDCAN1 interrupt line 0      */
  .word     handle_interrupt                  /* FDCAN2 interrupt line 0      */
  .word     handle_interrupt                  /* FDCAN1 interrupt line 1      */
  .word     handle_interrupt                  /* FDCAN2 interrupt line 1      */
  .word     handle_interrupt                  /* External Line[9:5]s          */
  .word     handle_interrupt                  /* TIM1 Break interrupt         */
  .word     handle_interrupt                  /* TIM1 Update interrupt        */
  .word     handle_interrupt                  /* TIM1 Trigger and Commutation interrupt */
  .word     handle_interrupt                  /* TIM1 Capture Compare         */
  .word     handle_interrupt                  /* TIM2                         */
  .word     handle_interrupt                  /* TIM3                         */
  .word     handle_interrupt                  /* TIM4                         */
  .word     handle_interrupt                  /* I2C1 Event                   */
  .word     handle_interrupt                  /* I2C1 Error                   */
  .word     handle_interrupt                  /* I2C2 Event                   */
  .word     handle_interrupt                  /* I2C2 Error                   */
  .word     handle_interrupt                  /* SPI1                         */
  .word     handle_interrupt                  /* SPI2                         */
  .word     handle_interrupt                  /* USART1                       */
  .word     handle_interrupt                  /* USART2                       */
  .word     handle_interrupt                  /* USART3                       */
  .word     handle_interrupt                  /* External Line[15:10]s        */
  .word     handle_interrupt                  /* RTC Alarm (A and B) through EXTI Line */
  .word     0                                 /* Reserved                     */
  .word     handle_interrupt                  /* TIM8 Break and TIM12         */
  .word     handle_interrupt                  /* TIM8 Update and TIM13        */
  .word     handle_interrupt                  /* TIM8 Trigger and Commutation and TIM14 */
  .word     handle_interrupt                  /* TIM8 Capture Compare         */
  .word     handle_interrupt                  /* DMA1 Stream7                 */
  .word     handle_interrupt                  /* FMC                          */
  .word     handle_interrupt                  /* SDMMC1                       */
  .word     handle_interrupt                  /* TIM5                         */
  .word     handle_interrupt                  /* SPI3                         */
  .word     handle_interrupt                  /* UART4                        */
  .word     handle_interrupt                  /* UART5                        */
  .word     handle_interrupt                  /* TIM6 and DAC1&2 underrun errors */
  .word     handle_interrupt                  /* TIM7                         */
  .word     handle_interrupt                  /* DMA2 Stream 0                */
  .word     handle_interrupt                  /* DMA2 Stream 1                */
  .word     handle_interrupt                  /* DMA2 Stream 2                */
  .word     handle_interrupt                  /* DMA2 Stream 3                */
  .word     handle_interrupt                  /* DMA2 Stream 4                */
  .word     handle_interrupt                  /* Ethernet                     */
  .word     handle_interrupt                  /* Ethernet Wakeup through EXTI line */
  .word     handle_interrupt                  /* FDCAN calibration unit interrupt*/
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     handle_interrupt                  /* DMA2 Stream 5                */
  .word     handle_interrupt                  /* DMA2 Stream 6                */
  .word     handle_interrupt                  /* DMA2 Stream 7                */
  .word     handle_interrupt                  /* USART6                       */
  .word     handle_interrupt                  /* I2C3 event                   */
  .word     handle_interrupt                  /* I2C3 error                   */
  .word     handle_interrupt                  /* USB OTG HS End Point 1 Out   */
  .word     handle_interrupt                  /* USB OTG HS End Point 1 In    */
  .word     handle_interrupt                  /* USB OTG HS Wakeup through EXTI */
  .word     handle_interrupt                  /* USB OTG HS                   */
  .word     handle_interrupt                  /* DCMI, PSSI                   */
  .word     handle_interrupt                  /* CRYP                         */
  .word     handle_interrupt                  /* Hash and Rng                 */
  .word     handle_interrupt                  /* FPU                          */
  .word     handle_interrupt                  /* UART7                        */
  .word     handle_interrupt                  /* UART8                        */
  .word     handle_interrupt                  /* SPI4                         */
  .word     handle_interrupt                  /* SPI5                         */
  .word     handle_interrupt                  /* SPI6                         */
  .word     handle_interrupt                  /* SAI1                         */
  .word     handle_interrupt                  /* LTDC                         */
  .word     handle_interrupt                  /* LTDC error                   */
  .word     handle_interrupt                  /* DMA2D                        */
  .word     0                                 /* Reserved                     */
  .word     handle_interrupt                  /* OCTOSPI1                     */
  .word     handle_interrupt                  /* LPTIM1                       */
  .word     handle_interrupt                  /* HDMI_CEC                     */
  .word     handle_interrupt                  /* I2C4 Event                   */
  .word     handle_interrupt                  /* I2C4 Error                   */
  .word     handle_interrupt                  /* SPDIF_RX                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     handle_interrupt                  /* DMAMUX1 Overrun interrupt    */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     0                                 /* Reserved                     */
  .word     handle_interrupt                  /* DFSDM Filter0 Interrupt      */
  .word     handle_interrupt                  /* DFSDM Filter1 Interrupt      */
  .word     handle_interrupt                  /* DFSDM Filter2 Interrupt      */
  .word     handle_interrupt                  /* DFSDM Filter3 Interrupt      */
  .word     0                                 /* Reserved                     */
  .word     handle_interrupt                  /* Serial Wire Interface 1 global interrupt */
  .word     handle_interrupt                  /* TIM15 global Interrupt          */
  .word     handle_interrupt                  /* TIM16 global Interrupt          */
  .word     handle_interrupt                  /* TIM17 global Interrupt          */
  .word     handle_interrupt                  /* MDIOS Wakeup  Interrupt         */
  .word     handle_interrupt                  /* MDIOS global Interrupt          */
  .word     0                                 /* Reserved                        */
  .word     handle_interrupt                  /* MDMA global Interrupt           */
  .word     0                                 /* Reserved                        */
  .word     handle_interrupt                  /* SDMMC2 global Interrupt         */
  .word     handle_interrupt                  /* HSEM1 global Interrupt          */
  .word     0                                 /* Reserved                        */
  .word     handle_interrupt                  /* ADC3 global Interrupt           */
  .word     handle_interrupt                  /* DMAMUX Overrun interrupt        */
  .word     handle_interrupt                  /* BDMA Channel 0 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 1 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 2 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 3 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 4 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 5 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 6 global Interrupt */
  .word     handle_interrupt                  /* BDMA Channel 7 global Interrupt */
  .word     handle_interrupt                  /* COMP1 global Interrupt          */
  .word     handle_interrupt                  /* LP TIM2 global interrupt        */
  .word     handle_interrupt                  /* LP TIM3 global interrupt        */
  .word     handle_interrupt                  /* LP TIM4 global interrupt        */
  .word     handle_interrupt                  /* LP TIM5 global interrupt        */
  .word     handle_interrupt                  /* LP UART1 interrupt              */
  .word     0                                 /* Reserved                        */
  .word     handle_interrupt                  /* Clock Recovery Global Interrupt */
  .word     handle_interrupt                  /* ECC diagnostic Global Interrupt */
  .word     handle_interrupt                  /* SAI4 global interrupt           */
  .word     handle_interrupt                  /* Digital Temperature Sensor  interrupt */
  .word     0                                 /* Reserved                              */
  .word     handle_interrupt                  /* Interrupt for all 6 wake-up pins      */
  .word     handle_interrupt                  /* OCTOSPI2 Interrupt       */
  .word     handle_interrupt                  /* OTFDEC1 Interrupt        */
  .word     handle_interrupt                  /* OTFDEC2 Interrupt        */
  .word     handle_interrupt                  /* FMAC Interrupt           */
  .word     handle_interrupt                  /* CORDIC Interrupt         */
  .word     handle_interrupt                  /* UART9 Interrupt          */
  .word     handle_interrupt                  /* UART10 Interrupt         */
  .word     handle_interrupt                  /* I2C5 Event Interrupt     */
  .word     handle_interrupt                  /* I2C5 Error Interrupt     */
  .word     handle_interrupt                  /* FDCAN3 interrupt line 0  */
  .word     handle_interrupt                  /* FDCAN3 interrupt line 1  */
  .word     handle_interrupt                  /* TIM23 global interrupt   */
  .word     handle_interrupt                  /* TIM24 global interrupt   */

/*******************************************************************************
*
* Provide weak aliases for each Exception handler to the Default_Handler.
* As they are weak aliases, any function with the same name will override
* this definition.
*
*******************************************************************************/
   .weak      NMI_Handler
   .thumb_set NMI_Handler,Default_Handler

   .weak      HardFault_Handler
   .thumb_set HardFault_Handler,Default_Handler

   .weak      MemManage_Handler
   .thumb_set MemManage_Handler,Default_Handler

   .weak      BusFault_Handler
   .thumb_set BusFault_Handler,Default_Handler

   .weak      UsageFault_Handler
   .thumb_set UsageFault_Handler,Default_Handler

   .weak      SVC_Handler
   .thumb_set SVC_Handler,Default_Handler

   .weak      DebugMon_Handler
   .thumb_set DebugMon_Handler,Default_Handler

   .weak      PendSV_Handler
   .thumb_set PendSV_Handler,Default_Handler

   .weak      SysTick_Handler
   .thumb_set SysTick_Handler,Default_Handler

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

