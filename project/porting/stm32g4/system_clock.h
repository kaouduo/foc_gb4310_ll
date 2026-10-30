/**
 *  @file   system_clock.h
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

#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#include "system_status.h"

/**
 *  @brief  使用内部 HSI 和 PLL 配置 170 MHz 系统主频。
 *  @param  无。
 *  @return SYS_OK 表示切换成功；SYS_ERROR_TIMEOUT 表示硬件等待超时；SYS_ERROR_IO 表示频率不符。
 *  @note   仅复位启动时调用；配置 Range 1 Boost、Flash 4 等待周期和总线时钟；失败后不得启动业务，应停机或复位。
 */
Sys_Status System_Clock_Init(void);

#endif
