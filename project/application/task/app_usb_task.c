/**
 *  @file   app_usb_task.c
 *  @brief  调度 CDC 文本、完整日志记录及响应后的平台动作。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.1
 *  @par 文件所属
 *  application / task
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 静态任务串行处理回显及三条命令。
 *  2026/10/31 | kaouduo | V1.1 | 增加有界日志队列、有限发送及会话恢复。
 */
#include "app_usb_task.h"
#include <string.h>
#include "FreeRTOS.h"
#include "app_shell.h"
#include "app_task.h"
#include "bsp_platform.h"
#include "bsp_usb_cdc.h"
#include "task.h"

#define LOG_QUEUE_BYTES 1024U
#define LOG_HEADER_BYTES 2U
#define OUTPUT_TIMEOUT_MS 100U
#define ACTION_TIMEOUT_MS 200U
#define OUTPUT_READY (1UL << 17)

static uint8_t logQueue[LOG_QUEUE_BYTES];
static uint32_t logHead, logTail, logUsed, logCount, droppedLogs;
static TaskHandle_t outputTask;
static bool acceptingLogs = true;
/* 只由 USB 任务管理，超时不取消尚未完成的底层发送。 */
static uint32_t outputSession;
static bool txPending, displayInvalid, sessionEstablished;

static uint8_t input[64];

/* 绑定前只有 main/启动任务这一条生产路径，无消费者、无 ISR 日志。
 * 不调用调度前的 FreeRTOS 临界区；绑定后统一使用可嵌套的任务临界区。 */
static uint32_t QueueLock(void)
{
    if (outputTask == NULL) {
        return 0U;
    }
    taskENTER_CRITICAL();
    return 1U;
}
static void QueueUnlock(uint32_t locked)
{
    if (locked != 0U) {
        taskEXIT_CRITICAL();
    }
}

/* 调用者持有队列临界区。 */
static void DiscardLogs(void)
{
    droppedLogs += logCount;
    logHead = logTail = logUsed = logCount = 0U;
}

void App_USBOutput_Init(TaskHandle_t task)
{
    uint32_t irqMask = QueueLock();
    outputTask = task;
    acceptingLogs = true;
    outputSession = 0U;
    txPending = displayInvalid = sessionEstablished = false;
    QueueUnlock(irqMask);
}

void App_USBOutput_Stop(void)
{
    uint32_t irqMask = QueueLock();
    acceptingLogs = false;
    DiscardLogs();
    QueueUnlock(irqMask);
}

bool App_USBOutput_LogWrite(const char *data, size_t length)
{
    uint32_t irqMask = QueueLock();
    if (!acceptingLogs || length == 0U || length > APP_USB_LOG_MAX_LENGTH ||
        length + LOG_HEADER_BYTES > LOG_QUEUE_BYTES - logUsed) {
        ++droppedLogs;
        QueueUnlock(irqMask);
        return false;
    }
    logQueue[logHead] = (uint8_t)length;
    logQueue[(logHead + 1U) % LOG_QUEUE_BYTES] = (uint8_t)(length >> 8U);
    logHead = (logHead + LOG_HEADER_BYTES) % LOG_QUEUE_BYTES;
    uint32_t first = LOG_QUEUE_BYTES - logHead;
    if (first > length) {
        first = (uint32_t)length;
    }
    memcpy(logQueue + logHead, data, first);
    memcpy(logQueue, data + first, length - first);
    logHead = (logHead + (uint32_t)length) % LOG_QUEUE_BYTES;
    logUsed += (uint32_t)length + LOG_HEADER_BYTES;
    ++logCount;
    /* 通知与关闭提交串行，禁止任务删除后使用旧句柄。 */
    if (outputTask != NULL) {
        (void)xTaskNotify(outputTask, OUTPUT_READY, eSetBits);
    }
    QueueUnlock(irqMask);
    return true;
}

uint32_t App_USBOutput_DroppedLogs(void)
{
    uint32_t irqMask = QueueLock();
    uint32_t count = droppedLogs;
    QueueUnlock(irqMask);
    return count;
}

static bool LogsPending(void)
{
    uint32_t irqMask = QueueLock();
    bool pending = logCount != 0U;
    QueueUnlock(irqMask);
    return pending;
}

static uint32_t WaitEvents(TickType_t ticks)
{
    uint32_t events = 0U;
    (void)xTaskNotifyWait(0U, UINT32_MAX, &events, ticks);
    if ((events & USB_CDC_EVENT_TX_DONE) != 0U) {
        txPending = false;
    }
    if ((events & USB_CDC_EVENT_ERROR) != 0U) {
        displayInvalid = true;
    }
    return events;
}

/* 会话检查与提交处于同一短临界区，禁止旧响应跨总线复位重放。 */
static Sys_Status WriteBlock(uint32_t session, const uint8_t *data, uint32_t length)
{
    uint32_t irqMask = QueueLock();
    Sys_Status status = BSP_USB_CDC_SessionId() == session ? BSP_USB_CDC_Write(data, length) : SYS_ERROR_NOT_READY;
    QueueUnlock(irqMask);
    return status;
}

static Sys_Status SendBytes(uint32_t session, const uint8_t *data, uint32_t length, uint32_t timeout,
                            uint32_t *completed)
{
    uint32_t started = BSP_Platform.timeMs();
    uint32_t offset = 0U;
    *completed = 0U;
    (void)WaitEvents(0U);
    for (;;) {
        if (BSP_USB_CDC_SessionId() != session) {
            return SYS_ERROR_NOT_READY;
        }
        if (displayInvalid) {
            return SYS_ERROR_IO;
        }
        if (!txPending) {
            *completed = offset;
            if (offset == length) {
                return SYS_OK;
            }
        }
        if (BSP_Platform.timeMs() - started >= timeout) {
            return SYS_ERROR_TIMEOUT;
        }
        if (!txPending) {
            uint32_t count = length - offset;
            if (count > 64U) {
                count = 64U;
            }
            Sys_Status status = WriteBlock(session, data + offset, count);
            if (status == SYS_OK) {
                offset += count;
                txPending = true;
            } else if (status != SYS_ERROR_BUSY && status != SYS_ERROR_NOT_READY) {
                return status;
            }
        }
        (void)WaitEvents(pdMS_TO_TICKS(1U));
    }
}

Sys_Status App_USBOutput_Write(const uint8_t *data, uint32_t length, uint32_t *completed)
{
    Sys_Status status = SendBytes(outputSession, data, length, OUTPUT_TIMEOUT_MS, completed);
    if (status != SYS_OK) {
        displayInvalid = true;
    }
    return status;
}

/* 消费前不释放记录空间，生产者不能覆盖正在发送的片段。 */
static void SendLog(void)
{
    uint32_t irqMask = QueueLock();
    uint32_t length = logQueue[logTail] | ((uint32_t)logQueue[(logTail + 1U) % LOG_QUEUE_BYTES] << 8U);
    uint32_t start = (logTail + LOG_HEADER_BYTES) % LOG_QUEUE_BYTES;
    QueueUnlock(irqMask);
    uint32_t first = LOG_QUEUE_BYTES - start;
    if (first > length) {
        first = length;
    }
    uint32_t completed;
    uint32_t started = BSP_Platform.timeMs();
    bool displayReady = App_Shell_BeforeLog();
    Sys_Status status = displayReady ? SendBytes(outputSession, logQueue + start, first, OUTPUT_TIMEOUT_MS, &completed)
                                     : SYS_ERROR_IO;
    if (status == SYS_OK && first != length) {
        uint32_t elapsed = BSP_Platform.timeMs() - started;
        status = elapsed < OUTPUT_TIMEOUT_MS
                ? SendBytes(outputSession, logQueue, length - first, OUTPUT_TIMEOUT_MS - elapsed, &completed)
                : SYS_ERROR_TIMEOUT;
    }
    irqMask = QueueLock();
    if (status == SYS_OK && BSP_USB_CDC_SessionId() == outputSession) {
        logTail = (logTail + length + LOG_HEADER_BYTES) % LOG_QUEUE_BYTES;
        logUsed -= length + LOG_HEADER_BYTES;
        --logCount;
    } else {
        DiscardLogs();
        displayInvalid = true;
    }
    QueueUnlock(irqMask);
    if (status == SYS_OK) {
        App_Shell_AfterLog();
        if (App_Shell_OutputFailed()) {
            displayInvalid = true;
        }
    }
}

static uint16_t ShellWrite(const void *data, uint16_t length)
{
    uint32_t completed;
    (void)App_USBOutput_Write(data, length, &completed);
    return (uint16_t)completed;
}

static void ExecuteAction(App_USBAction action)
{
    uint32_t accepted = BSP_Platform.timeMs();
    App_USBOutput_Stop();
    const char *response = action == APP_USB_ACTION_REBOOT ? "OK reboot\r\n" : "OK dfu\r\n";
    uint32_t completed;
    bool sent = SendBytes(outputSession, (const uint8_t *)response, (uint32_t)strlen(response), ACTION_TIMEOUT_MS,
                          &completed) == SYS_OK;
    uint32_t elapsed = BSP_Platform.timeMs() - accepted;
    if (!sent && elapsed < ACTION_TIMEOUT_MS) {
        (void)BSP_Platform.delayMs(ACTION_TIMEOUT_MS - elapsed);
    }
    if (action == APP_USB_ACTION_REBOOT) {
        BSP_Platform_Reset();
    } else {
        BSP_Platform_EnterDFU();
    }
}

void App_USBTask(void *argument)
{
    (void)argument;
    uint32_t events = 0U;
    do {
        (void)xTaskNotifyWait(0U, UINT32_MAX, &events, portMAX_DELAY);
    } while ((events & APP_USB_TASK_START) == 0U);
    uint32_t session = BSP_USB_CDC_SessionId();
    outputSession = session;
    uint32_t length = 0U, offset = 0U;
    bool shellOpen = false;
    for (;;) {
        uint32_t current = BSP_USB_CDC_SessionId();
        if (current != session) {
            uint32_t irqMask = QueueLock();
            if (sessionEstablished) {
                DiscardLogs();
            }
            QueueUnlock(irqMask);
            length = offset = 0U;
            session = outputSession = current;
            txPending = false;
            shellOpen = false;
        }
        if (displayInvalid) {
            uint32_t irqMask = QueueLock();
            if (sessionEstablished) {
                DiscardLogs();
            }
            QueueUnlock(irqMask);
            length = offset = 0U;
            shellOpen = false;
            displayInvalid = false;
        }
        if (!BSP_USB_CDC_IsTerminalOpen() || (!shellOpen && txPending)) {
            if (shellOpen) {
                uint32_t irqMask = QueueLock();
                DiscardLogs();
                QueueUnlock(irqMask);
                shellOpen = false;
                length = offset = 0U;
            }
            (void)WaitEvents(portMAX_DELAY);
            continue;
        }
        if (!shellOpen) {
            if (sessionEstablished) {
                uint32_t irqMask = QueueLock();
                DiscardLogs();
                QueueUnlock(irqMask);
            }
            Sys_Status status = App_Shell_Open(ShellWrite);
            if (status != SYS_OK) {
                /* 固定库配置错误属于初始化失败，输出错误等待下一次事件恢复。 */
                (void)WaitEvents(pdMS_TO_TICKS(1U));
                continue;
            }
            shellOpen = true;
            sessionEstablished = true;
        }
        if (offset == length) {
            uint32_t irqMask = QueueLock();
            if (BSP_USB_CDC_SessionId() == session) {
                (void)BSP_USB_CDC_Read(input, sizeof(input), &length);
            } else {
                length = 0U;
            }
            QueueUnlock(irqMask);
            offset = 0U;
        }
        bool consumedInput = false;
        if (BSP_USB_CDC_SessionId() == session && offset < length) {
            /* 命令可能等待输出，禁止在临界区调用 Shell。 */
            App_Shell_Feed(input[offset++]);
            consumedInput = true;
        }
        if (BSP_USB_CDC_SessionId() != session) {
            continue;
        }
        App_USBAction action = App_Shell_PendingAction();
        if (action != APP_USB_ACTION_NONE) {
            ExecuteAction(action);
        }
        if (App_Shell_OutputFailed()) {
            displayInvalid = true;
            continue;
        }
        if (LogsPending() && BSP_USB_CDC_IsConfigured()) {
            SendLog();
        } else if (offset == length && !consumedInput) {
            (void)WaitEvents(portMAX_DELAY);
        }
    }
}
