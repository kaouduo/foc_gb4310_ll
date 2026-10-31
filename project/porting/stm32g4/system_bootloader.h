/**
 *  @file   system_bootloader.h
 *  @brief  声明 G431 出厂 ROM 的最终平台交接接口。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *  @par 文件所属
 *  porting / stm32g4
 */
#ifndef SYSTEM_BOOTLOADER_H
#define SYSTEM_BOOTLOADER_H
/**
 *  @brief  清理运行环境并跳转 0x1FFF0000 的 ROM 向量。
 *  @param  无。
 *  @return 不返回；ROM 向量或时钟硬件状态异常时软件复位。
 *  @note   任务上下文、特权模式调用；调用前停止控制业务、断开 USB 并完成主机断开等待。
 *          不写 Flash/选项字节，不使用调度器退出后的任务延时。
 */
_Noreturn void System_Bootloader_Enter(void);
#endif
