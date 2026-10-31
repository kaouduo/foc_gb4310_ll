/**
 *  @file   system_bootloader.c
 *  @brief  为 G431 ROM Bootloader 清理时钟、中断和外设。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *  @par 文件所属
 *  porting / stm32g4
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 按 AN2606 的 G431 ROM 地址提供软件交接。
 */
#include "system_bootloader.h"
#include "stm32g4xx.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_system.h"

#define ROM_BASE 0x1FFF0000UL
#define CLOCK_WAIT_LIMIT 1000000U

/* 只在汇编中切换 MSP，之后不再返回 C 调用栈。 */
extern _Noreturn void System_Bootloader_Jump(uint32_t stack, uint32_t entry);

_Noreturn void System_Bootloader_Enter(void)
{
    const uint32_t stack = *(volatile const uint32_t *)ROM_BASE;
    const uint32_t entry = *(volatile const uint32_t *)(ROM_BASE + 4U);
    if ((stack & 7U) != 0U || stack <= SRAM_BASE || stack > SRAM_BASE + 32768U || (entry & 1U) == 0U ||
        entry < ROM_BASE || entry >= ROM_BASE + 0x7000U) {
        NVIC_SystemReset();
    }
    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
    for (uint32_t i = 0U; i < 8U; ++i) {
        NVIC->ICER[i] = UINT32_MAX;
        NVIC->ICPR[i] = UINT32_MAX;
    }
    /* 当前工程无运行中的看门狗；未来启用后需重新审查 ROM 交接契约。 */
    LL_RCC_HSI_Enable();
    uint32_t timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_HSI_IsReady() == 0U) {
        if (--timeout == 0U) {
            NVIC_SystemReset();
        }
    }
    RCC->CFGR = LL_RCC_SYS_CLKSOURCE_HSI;
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {
        if (--timeout == 0U) {
            NVIC_SystemReset();
        }
    }
    LL_RCC_PLL_Disable();
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_PLL_IsReady() != 0U) {
        if (--timeout == 0U) {
            NVIC_SystemReset();
        }
    }
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_ALL);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_ALL);
    LL_APB1_GRP2_ForceReset(LL_APB1_GRP2_PERIPH_ALL);
    LL_APB1_GRP2_ReleaseReset(LL_APB1_GRP2_PERIPH_ALL);
    LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_ALL);
    LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_ALL);
    LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_DMA1 | LL_AHB1_GRP1_PERIPH_DMA2 | LL_AHB1_GRP1_PERIPH_DMAMUX1);
    LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_DMA1 | LL_AHB1_GRP1_PERIPH_DMA2 | LL_AHB1_GRP1_PERIPH_DMAMUX1);
    LL_AHB2_GRP1_ForceReset(LL_AHB2_GRP1_PERIPH_ALL);
    LL_AHB2_GRP1_ReleaseReset(LL_AHB2_GRP1_PERIPH_ALL);

    RCC->CR = RCC_CR_HSION;
    RCC->CR = RCC_CR_HSION;
    RCC->PLLCFGR = 16U << RCC_PLLCFGR_PLLN_Pos;
    RCC->CRRCR = 0U;
    RCC->CIER = 0U;
    RCC->CICR = RCC_CICR_LSIRDYC | RCC_CICR_LSERDYC | RCC_CICR_HSIRDYC | RCC_CICR_HSERDYC | RCC_CICR_PLLRDYC |
            RCC_CICR_HSI48RDYC | RCC_CICR_CSSC | RCC_CICR_LSECSSC;
    RCC->CCIPR = 0U;
    RCC->CCIPR2 = 0U;
    RCC->AHB1ENR = 0U;
    RCC->AHB2ENR = 0U;
    RCC->APB1ENR1 = 0U;
    RCC->APB1ENR2 = 0U;
    RCC->APB2ENR = RCC_APB2ENR_SYSCFGEN;
    SystemCoreClock = 16000000U;
    for (uint32_t i = 0U; i < sizeof(NVIC->IP); ++i) {
        NVIC->IP[i] = 0U;
    }
    for (uint32_t i = 0U; i < sizeof(SCB->SHP); ++i) {
        SCB->SHP[i] = 0U;
    }
    SCB->SHCSR = 0U;
    SCB->SCR = 0U;
    SCB->VTOR = ROM_BASE;
    LL_SYSCFG_SetRemapMemory(LL_SYSCFG_REMAP_SYSTEMFLASH);
    __DSB();
    __ISB();
    System_Bootloader_Jump(stack, entry);
}
