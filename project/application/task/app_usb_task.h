/**
 * @file app_usb_task.h
 * @brief USB 任务输出资源与日志记录提交接口。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef APP_USB_TASK_H
#define APP_USB_TASK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "system_status.h"
#include "task.h"

#define APP_USB_LOG_MAX_LENGTH 192U

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 绑定唯一消费任务，保留启动前已提交的记录。
 * @param task 已创建且正在等待 START 的 USB 任务句柄。
 * @note 启动任务在 CDC 初始化前调用一次；绑定前仅允许 main/启动任务生产日志。
 */
/* 绑定任务时保留启动前的队列和统计；只绑定一次，不用于会话重置。 */
void App_USBOutput_Init(TaskHandle_t task);

/** @brief CDC 初始化失败时关闭记录提交，再由启动任务删除 USB 任务。 */
void App_USBOutput_Stop(void);

/**
 * @brief 将完整日志复制入队并唤醒 USB 任务，不等待 USB。
 * @param data 有效的 length 字节记录，返回后可复用。
 * @param length 1 至 APP_USB_LOG_MAX_LENGTH 字节。
 * @return true 为完整接受；false 为关闭、长度无效或队列满，累计丢弃条数。
 * @note 允许调度前及普通任务调用；不格式化、不截断。会话取消及输出失败作废的记录也计为丢弃。
 */
bool App_USBOutput_LogWrite(const char *data, size_t length);

/** @brief 获取本次上电累计丢弃日志条数，任务上下文调用。 */
uint32_t App_USBOutput_DroppedLogs(void);

/**
 * @brief 在 USB 任务内分包发送普通输出，最多等待 100 ms。
 * @param data 有效的 length 字节数据，调用期间保持不变。
 * @param length 待输出字节数，允许为零。
 * @param completed 返回已收到 TX_DONE 的字节数，不代表主机应用读取。
 * @return SYS_OK 为全部完成；其他返回码表示应终止当前显示并重建输入状态。
 * @note 仅唯一 USB 任务调用；失败后不重发本次数据，已提交但未完成的包不能撤回。
 */
Sys_Status App_USBOutput_Write(const uint8_t *data, uint32_t length, uint32_t *completed);

#ifdef __cplusplus
}
#endif

#endif /* APP_USB_TASK_H */
