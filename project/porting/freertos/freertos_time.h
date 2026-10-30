/**
 *  @file   freertos_time.h
 *  @brief  适配 FreeRTOS tick 时间及任务阻塞延时。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  porting / freertos
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 接入 FreeRTOS，将 DWT 测量与内核系统时间分开。
 */

#ifndef FREERTOS_TIME_H
#define FREERTOS_TIME_H

#include <stdint.h>
#include "system_status.h"

/**
 *  @brief  读取 FreeRTOS 调度器运行毫秒数。
 *  @param  无。
 *  @return 1 kHz tick 对应的 32 位毫秒值，约 49.7 天回绕；调度前为 0。
 *  @note   从调度器启动计时，不是日历时间；中断调用须满足 FromISR 优先级限制，不支持 NMI 或高优先级控制中断，长时间屏蔽
 * tick 会影响计时。
 */
uint32_t System_TimeMs(void);
/**
 *  @brief  阻塞当前任务指定的毫秒 tick 数。
 *  @param  milliseconds 阻塞 tick 数，1 kHz 下单位为 ms，0 表示主动让出同优先级执行机会。
 *  @return SYS_OK 表示当前任务已恢复执行。
 *  @note   调用方保证调度器运行且处于可阻塞的任务上下文；不检查上下文，实际时间受 tick 相位和调度延迟影响。
 */
Sys_Status System_DelayMs(uint32_t milliseconds);

#endif
