/**
 *  @file   system_bootloader_jump.S
 *  @brief  丢弃任务 PSP，切换 ROM MSP 并跳转；不返回旧 C 栈。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *  @par 文件所属
 *  porting / stm32g4
 */
.syntax unified
.cpu cortex-m4
.thumb
.section .text.System_Bootloader_Jump,"ax",%progbits
.global System_Bootloader_Jump
.type System_Bootloader_Jump, %function
.thumb_func
System_Bootloader_Jump:
    movs r2, #0
    msr basepri, r2
    msr faultmask, r2
    msr control, r2
    isb
    msr msp, r0
    dsb
    isb
    cpsie i
    bx r1
.size System_Bootloader_Jump, .-System_Bootloader_Jump
