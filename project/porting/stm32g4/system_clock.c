/**
 *  @file   system_clock.c
 *  @brief  配置内部 HSI 的 170 MHz 主频及 USB 独立时钟。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  porting / stm32g4
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 实现内部 HSI 最高主频、系统计时及受限 Flash 参数存储。
 *  2026/10/31 | kaouduo | V1.1 | 系统初始化统一配置 USB HSI48 时钟与 SOF 自动校准。
 */

#include "system_clock.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_crs.h"
#include "stm32g4xx_ll_pwr.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_system.h"

#define CLOCK_WAIT_LIMIT 1000000U

/**
 *  @brief  配置 HSI/PLL 170 MHz 主频和 HSI48/CRS USB 时钟。
 *  @param  无。
 *  @return SYS_OK 表示切换成功；SYS_ERROR_TIMEOUT 表示硬件等待超时；SYS_ERROR_IO 表示频率不符。
 *  @note   仅复位启动时调用；配置 Range 1 Boost、Flash 4 等待周期、USB 时钟及 CRS；不等待 SOF，失败后停止启动。
 */
Sys_Status System_Clock_Init(void)
{
    uint32_t timeout;

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    /* 四位优先级全部用作抢占优先级，满足 FreeRTOS Cortex-M4F 移植要求。 */
    NVIC_SetPriorityGrouping(3U);
    /** 配置内部稳压器输出电压。 */
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_PWR_IsActiveFlag_VOS() != 0U) {
        if (--timeout == 0U) {
            return SYS_ERROR_TIMEOUT;
        }
    }
    LL_PWR_EnableRange1BoostMode();

    /** 配置 Flash 等待周期。 */
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4) {
        if (--timeout == 0U) {
            return SYS_ERROR_TIMEOUT;
        }
    }

    /** 启用 HSI 振荡器。 */
    LL_RCC_HSI_Enable();
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_HSI_IsReady() != 1U) {
        if (--timeout == 0U) {
            return SYS_ERROR_TIMEOUT;
        }
    }

    /** 配置并启用系统 PLL。 */
    /* 保留出厂 HSI 校准，PLL 输入 4 MHz、VCO 340 MHz、R 输出 170 MHz。 */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_4, 85U, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_EnableDomain_SYS();
    LL_RCC_PLL_Enable();
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_PLL_IsReady() != 1U) {
        if (--timeout == 0U) {
            return SYS_ERROR_TIMEOUT;
        }
    }

    /** 配置 CPU、AHB 和 APB 总线时钟。 */
    /* 先经过 85 MHz 中间状态，满足升至 170 MHz 时的至少 1 us 过渡要求。 */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_2);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
        if (--timeout == 0U) {
            return SYS_ERROR_TIMEOUT;
        }
    }
    for (volatile uint32_t i = 0; i < 100U; ++i) {
        __NOP();
    }
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    __DSB();
    __ISB();

    /** 配置 USB 独立的 48 MHz 时钟。 */
    LL_RCC_HSI48_Enable();
    timeout = CLOCK_WAIT_LIMIT;
    while (LL_RCC_HSI48_IsReady() != 1U) {
        if (--timeout == 0U) {
            return SYS_ERROR_TIMEOUT;
        }
    }
    LL_RCC_SetUSBClockSource(LL_RCC_USB_CLKSOURCE_HSI48);

    /** 配置 CRS，以主机 1 kHz SOF 自动校准 HSI48。 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CRS);
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_CRS);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_CRS);
    LL_CRS_SetSyncDivider(LL_CRS_SYNC_DIV_1);
    LL_CRS_SetSyncSignalSource(LL_CRS_SYNC_SOURCE_USB);
    LL_CRS_SetSyncPolarity(LL_CRS_SYNC_POLARITY_RISING);
    LL_CRS_SetReloadCounter(47999U);
    LL_CRS_SetFreqErrorLimit(34U);
    LL_CRS_SetHSI48SmoothTrimming(32U);
    LL_CRS_EnableAutoTrimming();
    LL_CRS_EnableFreqErrorCounter();

    /** 更新并校验系统核心时钟与 USB 时钟源，不等待主机连接。 */
    SystemCoreClockUpdate();
    return SystemCoreClock == 170000000U && LL_RCC_GetUSBClockSource(LL_RCC_USB_CLKSOURCE) == LL_RCC_USB_CLKSOURCE_HSI48
            ? SYS_OK
            : SYS_ERROR_IO;
}
