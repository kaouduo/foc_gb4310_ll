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
#include "app_task.h"
#include "app_usb_console.h"
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
    Sys_Status status = SendBytes(outputSession, logQueue + start, first, OUTPUT_TIMEOUT_MS, &completed);
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
}

static App_USBConsole console;
static bool SendResponse(uint32_t session, App_USBAction action)
{
    uint32_t completed;
    if (action != APP_USB_ACTION_NONE) {
        App_USBOutput_Stop();
    }
    return SendBytes(session, console.response, console.responseLength,
                     action != APP_USB_ACTION_NONE ? ACTION_TIMEOUT_MS : OUTPUT_TIMEOUT_MS, &completed) == SYS_OK;
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
    App_USBConsole_Reset(&console);
    for (;;) {
        if (displayInvalid) {
            uint32_t locked = QueueLock();
            DiscardLogs();
            QueueUnlock(locked);
            App_USBConsole_Reset(&console);
            length = offset = 0U;
            displayInvalid = false;
        }
        uint32_t current = BSP_USB_CDC_SessionId();
        if (current != session) {
            App_USBConsole_Reset(&console);
            length = offset = 0U;
            session = outputSession = current;
            txPending = false;
            uint32_t locked = QueueLock();
            if (sessionEstablished) {
                DiscardLogs();
            }
            QueueUnlock(locked);
        }
        if (BSP_USB_CDC_IsConfigured()) {
            sessionEstablished = true;
        }
        if (offset == length) {
            taskENTER_CRITICAL();
            if (BSP_USB_CDC_SessionId() == session) {
                (void)BSP_USB_CDC_Read(input, sizeof(input), &length);
            } else {
                length = 0U;
            }
            taskEXIT_CRITICAL();
            offset = 0U;
        }
        bool consumedInput = false;
        taskENTER_CRITICAL();
        if (BSP_USB_CDC_SessionId() != session) {
            App_USBConsole_Reset(&console);
            length = offset = 0U;
            session = outputSession = BSP_USB_CDC_SessionId();
            txPending = false;
        } else if (offset < length) {
            if (App_USBConsole_Feed(&console, input[offset], BSP_Platform.timeMs())) {
                ++offset;
                consumedInput = true;
            }
        } else {
            App_USBConsole_Poll(&console, BSP_Platform.timeMs());
        }
        taskEXIT_CRITICAL();
        if (console.responseLength != 0U) {
            App_USBAction action = console.action;
            uint32_t accepted = BSP_Platform.timeMs();
            bool sent = SendResponse(session, action);
            if (action != APP_USB_ACTION_NONE) {
                uint32_t elapsed = BSP_Platform.timeMs() - accepted;
                if (!sent && elapsed < 200U) {
                    (void)BSP_Platform.delayMs(200U - elapsed);
                }
                if (action == APP_USB_ACTION_REBOOT) {
                    BSP_Platform_Reset();
                } else {
                    BSP_Platform_EnterDFU();
                }
            }
            if (sent) {
                App_USBConsole_Consumed(&console);
            } else {
                App_USBConsole_Reset(&console);
                length = offset = 0U;
            }
        } else if (LogsPending() && BSP_USB_CDC_IsConfigured()) {
            SendLog();
        } else if (offset == length && !consumedInput) {
            (void)xTaskNotifyWait(0U, UINT32_MAX, &events, console.crPending ? pdMS_TO_TICKS(1U) : portMAX_DELAY);
        }
    }
}
