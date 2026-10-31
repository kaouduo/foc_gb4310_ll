/**
 * @file app_shell.h
 * @brief root Shell 会话、输入及延后平台动作接口。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef APP_SHELL_H
#define APP_SHELL_H

#include <stdbool.h>
#include <stdint.h>
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 命令接受后，由 USB 任务确认响应并执行的动作。 */
typedef enum { APP_USB_ACTION_NONE, APP_USB_ACTION_REBOOT, APP_USB_ACTION_DFU } App_USBAction;
/** @brief 同步消费输出，返回已完成字节数；失败后终止本次显示。 */
typedef uint16_t (*App_ShellWrite)(const void *data, uint16_t length);

/** @brief USB 任务在有效会话中建立 root 免密码 Shell，清空历史并显示提示符。 */
Sys_Status App_Shell_Open(App_ShellWrite write);
/** @brief USB 任务输入一个原始字节；处理分包按键、CRLF 去重和超长行拒绝。 */
void App_Shell_Feed(uint8_t byte);
/** @brief 仅 USB 任务调用：清除显示行、日志后恢复当前编辑内容和光标。 */
bool App_Shell_BeforeLog(void);
void App_Shell_AfterLog(void);
/** @brief 查询本次显示是否失败；由 USB 任务等待传输恢复后重开会话。 */
bool App_Shell_OutputFailed(void);
/** @brief 查询已接受的复位/DFU 请求，Open 时复位为 NONE。 */
App_USBAction App_Shell_PendingAction(void);
/** @brief clear 命令请求；在当前 REPL 返回后清屏并重建历史。 */
void App_Shell_RequestClear(void);
/** @brief 命令只登记平台动作，不在命令函数内复位或跳转。 */
void App_Shell_RequestAction(App_USBAction action);

#ifdef __cplusplus
}
#endif
#endif /* APP_SHELL_H */
