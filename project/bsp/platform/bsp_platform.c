/**
 *  @file   bsp_platform.c
 *  @brief  静态绑定平台常用能力，提供常用能力绑定和低频平台操作。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.2
 *
 *  @par 文件所属
 *  BSP / platform
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.2 | 用只读函数指针结构绑定常用接口，移除无附加逻辑的转发。
 */

#include "bsp_platform.h"
#include "bsp_usb_cdc.h"
#include "dwt.h"
#include "freertos_time.h"
#include "system_bootloader.h"
#include "system_control.h"

/* 类型在编译期核对，直接绑定底层实现，避免同签名函数的逐项转发。 */
const BSP_PlatformTypedef BSP_Platform = {
        .timeMs = System_TimeMs,
        .delayMs = System_DelayMs,
        .dwtCycles = DWT_Cycles,
        .dwtElapsedUs = DWT_ElapsedUs,
        .dwtDelayUs = DWT_DelayUs,
};

/**
 *  @brief  读取内核 HCLK 的软件频率值。
 *  @param  无。
 *  @return SystemCoreClock，单位 Hz，初始化后为 170000000。
 *  @note   系统时钟已配置并更新；不测量 HSI 的实际频偏，无副作用。
 */
uint32_t BSP_Platform_ClockHz(void) { return System_ClockHz(); }

/**
 *  @brief  请求芯片软件复位。
 *  @param  无。
 *  @return 不返回。
 *  @note   调用前停止电机并按需保存参数；不自动写 Flash，当前任务和调度器均不会继续运行。
 */
void BSP_Platform_Reset(void) { System_Reset(); }

/**
 *  @brief  读取芯片 96 位唯一标识。
 *  @param  words 可写的三个 uint32_t 元素缓冲区。
 *  @return SYS_OK 表示读取完成。
 *  @note   可在任务或启动阶段使用；调用者保证三个元素的缓冲区有效，不改变硬件配置。
 */
Sys_Status BSP_Platform_UniqueId(uint32_t words[3]) { return System_UniqueId(words); }

void BSP_Platform_EnterDFU(void)
{
    BSP_USB_CDC_Deinit();
    BSP_USB_CDC_Prepare();
    System_Bootloader_Enter();
}
