/**
 *  @file   system_clock.h
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

#ifndef SYSTEM_CLOCK_H
#define SYSTEM_CLOCK_H

#include "system_status.h"

/**
 *  @brief  配置 HSI/PLL 170 MHz 主频和 HSI48/CRS USB 时钟。
 *  @param  无。
 *  @return SYS_OK 表示切换成功；SYS_ERROR_TIMEOUT 表示硬件等待超时；SYS_ERROR_IO 表示频率不符。
 *  @note   仅复位启动时调用；配置 Range 1 Boost、Flash 4 等待周期、USB 时钟及 CRS；不等待 SOF，失败后停止启动。
 */
Sys_Status System_Clock_Init(void);

#endif
