/* Cortex-M 异常入口：不使用栈，先关闭 TIM1 MOE 和相输出，再停机保留故障寄存器。
 * STM32G431 TIM1 基址 0x40012C00，CCER=0x20，BDTR=0x44。
 * author: kaouduo, 2026/10/31 */
.syntax unified
.cpu cortex-m4
.thumb
.section .text.Motor_FaultShutdown,"ax",%progbits
.global HardFault_Handler
.global MemManage_Handler
.global BusFault_Handler
.global UsageFault_Handler
.global NMI_Handler
.type HardFault_Handler,%function
.thumb_func
HardFault_Handler:
.thumb_func
MemManage_Handler:
.thumb_func
BusFault_Handler:
.thumb_func
UsageFault_Handler:
.thumb_func
NMI_Handler:
    cpsid i
    ldr r0, =0x40012C44
    ldr r1, [r0]
    bic r1, r1, #0x8000
    str r1, [r0]
    ldr r0, =0x40012C20
    ldr r1, [r0]
    ldr r2, =0x555
    bic r1, r1, r2
    str r1, [r0]
    dsb
1:  b 1b
.size HardFault_Handler, .-HardFault_Handler
