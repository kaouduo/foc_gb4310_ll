/**
 *  @file   system_control.c
 *  @brief  实现系统频率查询、空闲、复位和唯一标识读取。
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

#include "system_control.h"
#include "stm32g4xx.h"

/**
 *  @brief  读取当前内核 HCLK 频率。
 *  @param  无。
 *  @return SystemCoreClock 保存的频率，单位 Hz，初始化成功后为 170000000。
 *  @note   不读取或修改时钟寄存器；必须已完成系统时钟更新，不能用于测量 HSI 实际频偏。
 */
uint32_t System_ClockHz(void) { return SystemCoreClock; }
/* DWT 在内核睡眠时不保证计数，保持运行以维持统一时间基准。 */
/**
 *  @brief  执行保持内核运行的空闲指令。
 *  @param  无。
 *  @return 无返回值。
 *  @note   主循环中调用；不进入睡眠，以保持 DWT 时间基准连续；无失败状态。
 */
void System_Idle(void) { __NOP(); }
/**
 *  @brief  请求 Cortex-M 系统软件复位。
 *  @param  无。
 *  @return 不返回，芯片进入复位流程。
 *  @note   复位前由业务停止电机并完成必要的数据保存；不自动提交 Flash 数据，也不恢复当前执行现场。
 */
void System_Reset(void) { NVIC_SystemReset(); }
/**
 *  @brief  读取芯片的 96 位唯一标识。
 *  @param  words 可写的三个 uint32_t 元素缓冲区，按地址递增顺序保存 UID。
 *  @return SYS_OK 表示三个字均已读取。
 *  @note   调用者保证三个元素的缓冲区有效；不检查空指针，不修改芯片配置。
 */
Sys_Status System_UniqueId(uint32_t words[3])
{
    for (uint32_t i = 0U; i < 3U; ++i) {
        words[i] = *(volatile const uint32_t *)(UID_BASE + i * 4U);
    }
    return SYS_OK;
}
