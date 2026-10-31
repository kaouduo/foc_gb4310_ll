/**
 *  @file   app_usb_console.h
 *  @brief  定义静态文本解析状态与响应接口。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *  @par 文件所属
 *  application / usb
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 实现阶段 4 文本通信与平台交接。
 */
#ifndef APP_USB_CONSOLE_H
#define APP_USB_CONSOLE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief 完整命令接受后，由 USB 任务在响应完成或超时后执行。 */
typedef enum { APP_USB_ACTION_NONE, APP_USB_ACTION_REBOOT, APP_USB_ACTION_DFU } App_USBAction;
/** @brief 单任务持有；最多 128 B 正文，响应存储不使用任务栈。 */
typedef struct {
    uint8_t line[130];
    uint8_t response[130];
    uint32_t length, responseLength, crTime;
    bool overflow, crPending, suppressLF;
    App_USBAction action;
} App_USBConsole;
/** @brief 清除旧会话文本及响应；已接受的复位动作由任务独立保存。 */
void App_USBConsole_Reset(App_USBConsole *console);
/** @brief 输入一个字节；有待发送响应时返回 false，由调用方保留该字节。 */
bool App_USBConsole_Feed(App_USBConsole *console, uint8_t byte, uint32_t nowMs);
/** @brief 裸 CR 等待 10 ms 后提交；支持毫秒计数回绕。 */
void App_USBConsole_Poll(App_USBConsole *console, uint32_t nowMs);
/** @brief 当前响应发送完成后允许继续解析。 */
void App_USBConsole_Consumed(App_USBConsole *console);
#ifdef __cplusplus
}
#endif
#endif
