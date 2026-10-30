/**
 *  @file   system_clock.c
 *  @brief  配置基于内部 HSI 的 170 MHz 系统时钟。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / stm32g4
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 实现内部 HSI 最高主频、系统计时及受限 Flash 参数存储。
 */

#include "system_clock.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_system.h"

#define CLOCK_WAIT_LIMIT 1000000U

/**
 *  @brief  有界轮询寄存器字段是否达到预期值。
 *  @param  reg 有效的硬件寄存器地址。
 *  @param  mask 需要检查的位掩码。
 *  @param  value 掩码内的预期位值。
 *  @return 1 表示字段匹配；0 表示达到 CLOCK_WAIT_LIMIT 次轮询仍未匹配。
 *  @note   仅启动阶段使用；循环次数不代表固定时间，不修改寄存器。
 */
static int WaitRegister(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    for (uint32_t i = 0; i < CLOCK_WAIT_LIMIT; ++i) {
        if ((*reg & mask) == value) {
            return 1;
        }
    }
    return 0;
}

/**
 *  @brief  使用内部 HSI 和 PLL 配置 170 MHz 系统主频。
 *  @param  无。
 *  @return SYS_OK 表示切换成功；SYS_ERROR_TIMEOUT 表示硬件等待超时；SYS_ERROR_IO 表示频率不符。
 *  @note   仅复位启动时调用；配置 Range 1 Boost、Flash 4 等待周期和总线时钟；失败后不得启动业务，应停机或复位。
 */
Sys_Status System_Clock_Init(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    /* 四位优先级全部用作抢占优先级，满足 FreeRTOS Cortex-M4F 移植要求。 */
    NVIC_SetPriorityGrouping(3U);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    if (!WaitRegister(&PWR->SR2, PWR_SR2_VOSF, 0U)) {
        return SYS_ERROR_TIMEOUT;
    }
    LL_PWR_EnableRange1BoostMode();
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    if (!WaitRegister(&FLASH->ACR, FLASH_ACR_LATENCY, LL_FLASH_LATENCY_4)) {
        return SYS_ERROR_TIMEOUT;
    }
    LL_RCC_HSI_Enable();
    if (!WaitRegister(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) {
        return SYS_ERROR_TIMEOUT;
    }
    /* 保留出厂 HSI 校准，PLL 输入 4 MHz、VCO 340 MHz、R 输出 170 MHz。 */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_4, 85U, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_EnableDomain_SYS();
    LL_RCC_PLL_Enable();
    if (!WaitRegister(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) {
        return SYS_ERROR_TIMEOUT;
    }
    /* 先经过 85 MHz 中间状态，满足升至 170 MHz 时的至少 1 us 过渡要求。 */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    if (!WaitRegister(&RCC->CFGR, RCC_CFGR_SWS, LL_RCC_SYS_CLKSOURCE_STATUS_PLL)) {
        return SYS_ERROR_TIMEOUT;
    }
    for (volatile uint32_t i = 0; i < 100U; ++i) {
        __NOP();
    }
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    __DSB();
    __ISB();
    SystemCoreClockUpdate();
    return SystemCoreClock == 170000000U ? SYS_OK : SYS_ERROR_IO;
}
