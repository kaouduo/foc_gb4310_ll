/**
 *  @file   bsp_platform.h
 *  @brief  静态绑定平台常用能力，提供组合初始化和低频平台操作。
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

#ifndef BSP_PLATFORM_H
#define BSP_PLATFORM_H

#include <stdint.h>
#include "system_status.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

/** @brief 平台常用能力；由 BSP 在编译期绑定 porting，运行中不可替换。 */
typedef struct {
    uint32_t (*timeMs)(void); /*!< 内核运行毫秒数，1 kHz tick，32 位约 49.7 天回绕；高优先级中断禁用。 */
    Sys_Status (*delayMs)(uint32_t milliseconds); /*!< 任务阻塞延时，单位 ms；0 表示让出执行机会，中断禁用。 */
    uint32_t (*dwtCycles)(void); /*!< DWT 原始周期数；初始化后可用，每约 25.26 秒回绕，不是系统时间。 */
    uint32_t (*dwtElapsedUs)(uint32_t startCycles);  /*!< 从起始周期到当前的耗时，单位 us；跨度须小于一次回绕。 */
    Sys_Status (*dwtDelayUs)(uint32_t microseconds); /*!< DWT 微秒忙等待；初始化完成后用于短硬件时序。 */
} BSP_PlatformTypedef;

/**
 * @brief 平台能力的只读板级实例。
 * @note 所有函数指针均已静态绑定且非空；使用前完成 BSP_Platform_Init。
 */
extern const BSP_PlatformTypedef BSP_Platform;

/**
 *  @brief  初始化系统主频和 DWT 测量。
 *  @param  无。
 *  @return SYS_OK 表示资源均初始化；其他负值透传底层具体错误。
 *  @note   复位后、模块 IO 和调度器启动前调用一次；失败后停止启动，成功也不启动任务或 Flash 擦写。
 */
Sys_Status BSP_Platform_Init(void);

/**
 *  @brief  读取内核 HCLK 的软件频率值。
 *  @param  无。
 *  @return SystemCoreClock，单位 Hz，初始化后为 170000000。
 *  @note   系统时钟已配置并更新；不测量 HSI 的实际频偏，无副作用。
 */
uint32_t BSP_Platform_ClockHz(void);

/**
 *  @brief  请求芯片软件复位。
 *  @param  无。
 *  @return 不返回。
 *  @note   调用前停止电机并按需保存参数；不自动写 Flash，当前任务和调度器均不会继续运行。
 */
void BSP_Platform_Reset(void);

/**
 *  @brief  读取芯片 96 位唯一标识。
 *  @param  words 可写的三个 uint32_t 元素缓冲区。
 *  @return SYS_OK 表示读取完成。
 *  @note   可在任务或启动阶段使用；调用者保证三个元素的缓冲区有效，不改变硬件配置。
 */
Sys_Status BSP_Platform_UniqueId(uint32_t words[3]);

#ifdef __cplusplus
}
#endif

#endif /* BSP_PLATFORM_H */
