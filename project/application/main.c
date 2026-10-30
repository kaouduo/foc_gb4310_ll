/**
 *  @file   main.c
 *  @brief  完成平台基础初始化。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  application / 启动编排
 */

#include "dwt.h"
#include "system_clock.h"
#include "system_control.h"

/**
 *  @brief  初始化系统时钟与 DWT，随后保持空闲。
 *  @param  无。
 *  @return 不返回，初始化失败时停机。
 *  @note   由芯片启动文件调用一次；此时尚无任务与调度器。
 */
int main(void)
{
    if (System_Clock_Init() != SYS_OK) {
        for (;;) {
        }
    }
    if (DWT_Init() != SYS_OK) {
        for (;;) {
        }
    }
    for (;;) {
        System_Idle();
    }
}
