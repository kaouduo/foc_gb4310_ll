/**
 *  @file   dwt.h
 *  @brief  提供 DWT 原始周期、耗时测量和微秒忙等待。
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
 *  2026/10/30 | kaouduo | V1.1 | 接入 FreeRTOS，将 DWT 测量与内核系统时间分开。
 */

#ifndef DWT_H
#define DWT_H
#include <stdint.h>
#include "system_status.h"
/**
 *  @brief  初始化 DWT 周期计数器。
 *  @param  无。
 *  @return SYS_OK 表示计数器可用；SYS_ERROR_NOT_READY 表示硬件计数器不可用或未运行。
 *  @note   在 170 MHz 时钟配置完成后调用一次；不配置 SysTick，不提供系统时间，失败时禁止测量和延时。
 */
Sys_Status DWT_Init(void);

/**
 *  @brief  读取 DWT 当前原始周期计数。
 *  @param  无。
 *  @return 32 位内核周期计数，每约 25.26 秒回绕。
 *  @note   先完成 DWT 初始化；可在任务和中断中读取，不是系统时间戳，不包含睡眠和调试暂停时间。
 */
uint32_t DWT_Cycles(void);

/**
 *  @brief  将一次 DWT 周期间隔换算为微秒。
 *  @param  startCycles 开始测量时保存的 DWT_Cycles 返回值。
 *  @return 耗时微秒数，不足 1 us 的部分截断。
 *  @note   测量跨度必须小于一次 32 位计数周期，支持跨越单次回绕；任务抢占耗时包含在结果内，不用于累计系统运行时间。
 */
uint32_t DWT_ElapsedUs(uint32_t startCycles);

/**
 *  @brief  使用 DWT 忙等待指定的微秒数。
 *  @param  microseconds 延时长度，单位 us，0 表示不等待。
 *  @return SYS_OK 表示等待完成。
 *  @note   先完成 DWT 初始化；占用 CPU，中断或任务抢占会延长延时，仅用于短硬件时序，长等待使用 vTaskDelay。
 */
Sys_Status DWT_DelayUs(uint32_t microseconds);
#endif
