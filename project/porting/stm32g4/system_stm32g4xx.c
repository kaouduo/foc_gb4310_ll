/**
 *  @file   system_stm32g4xx.c
 *  @brief  提供 CMSIS 系统初始化和内核时钟频率更新。
 *  @author MCD Application Team / kaouduo
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
 *  2026/10/30 | kaouduo | V1.0 | 按工程规范统一中文注释和代码格式，保持原有逻辑。
 */

/*
 * @attention
 *
 * Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "stm32g4xx.h"

#if !defined(HSE_VALUE)
#define HSE_VALUE 24000000U
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U
#endif

#if defined(USER_VECT_TAB_ADDRESS)

#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS SRAM_BASE
#else
#define VECT_TAB_BASE_ADDRESS FLASH_BASE
#endif

#if !defined(VECT_TAB_OFFSET)
#define VECT_TAB_OFFSET 0x00000000U
#endif

#endif

uint32_t SystemCoreClock = HSI_VALUE;

const uint8_t AHBPrescTable[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

/**
 *  @brief  配置内核浮点访问权限及可选的向量表地址。
 *  @param  无。
 *  @return 无返回值。
 *  @note   由启动汇编在 RAM 数据初始化前调用；修改 CPACR 和可选的 VTOR，不配置 PLL；无失败状态返回。
 */
void SystemInit(void)
{

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << (10 * 2)) | (3UL << (11 * 2)));
#endif

#if defined(USER_VECT_TAB_ADDRESS)
    SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET;
#endif
}

/**
 *  @brief  根据时钟寄存器更新 SystemCoreClock 的 HCLK 频率。
 *  @param  无。
 *  @return 无返回值，结果写入 SystemCoreClock，单位 Hz。
 *  @note   修改系统时钟后调用；HSI_VALUE 与 HSE_VALUE 必须符合实际频率；只更新软件变量，常量不符时结果不可用于计时。
 */
void SystemCoreClockUpdate(void)
{
    uint32_t tmp, pllvco, pllr, pllsource, pllm;

    switch (RCC->CFGR & RCC_CFGR_SWS) {
        case 0x04:
            SystemCoreClock = HSI_VALUE;
            break;

        case 0x08:
            SystemCoreClock = HSE_VALUE;
            break;

        case 0x0C:

            pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
            pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> 4) + 1U;
            if (pllsource == 0x02UL) {
                pllvco = (HSI_VALUE / pllm);
            } else {
                pllvco = (HSE_VALUE / pllm);
            }
            pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 8);
            pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> 25) + 1U) * 2U;
            SystemCoreClock = pllvco / pllr;
            break;

        default:
            break;
    }

    tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];

    SystemCoreClock >>= tmp;
}
