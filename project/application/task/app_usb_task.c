/**
 *  @file   app_usb_task.c
 *  @brief  调度 CDC 文本收发及响应后的平台动作。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *  @par 文件所属
 *  application / task
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 静态任务串行处理回显及三条命令。
 */
#include "FreeRTOS.h"
#include "app_task.h"
#include "app_usb_console.h"
#include "bsp_platform.h"
#include "bsp_usb_cdc.h"
#include "task.h"

static App_USBConsole console;
static uint8_t input[64];

/* 会话检查与提交处于同一短临界区，禁止旧响应跨总线复位重放。 */
static Sys_Status WriteBlock(uint32_t session, const uint8_t *data, uint32_t length)
{
    taskENTER_CRITICAL();
    Sys_Status status = BSP_USB_CDC_SessionId() == session ? BSP_USB_CDC_Write(data, length) : SYS_ERROR_NOT_READY;
    taskEXIT_CRITICAL();
    return status;
}

static bool SendResponse(uint32_t session, App_USBAction action)
{
    uint32_t started = BSP_Platform.timeMs();
    uint32_t offset = 0U;
    bool waiting = false;
    uint32_t events = 0U;
    (void)xTaskNotifyWait(0U, UINT32_MAX, &events, 0U);
    for (;;) {
        if (BSP_USB_CDC_SessionId() != session || (events & USB_CDC_EVENT_ERROR) != 0U) {
            return false;
        }
        if ((events & USB_CDC_EVENT_TX_DONE) != 0U) {
            waiting = false;
        }
        if (!waiting && offset == console.responseLength) {
            return true;
        }
        if (action != APP_USB_ACTION_NONE && BSP_Platform.timeMs() - started >= 200U) {
            return false;
        }
        if (!waiting) {
            uint32_t count = console.responseLength - offset;
            if (count > 64U) {
                count = 64U;
            }
            Sys_Status status = WriteBlock(session, console.response + offset, count);
            if (status == SYS_OK) {
                offset += count;
                waiting = true;
            } else if (status != SYS_ERROR_BUSY && status != SYS_ERROR_NOT_READY) {
                return false;
            }
        }
        (void)xTaskNotifyWait(0U, UINT32_MAX, &events, pdMS_TO_TICKS(1U));
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
    uint32_t length = 0U, offset = 0U;
    App_USBConsole_Reset(&console);
    for (;;) {
        uint32_t current = BSP_USB_CDC_SessionId();
        if (current != session) {
            App_USBConsole_Reset(&console);
            length = offset = 0U;
            session = current;
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
            session = BSP_USB_CDC_SessionId();
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
        } else if (offset == length && !consumedInput) {
            (void)xTaskNotifyWait(0U, UINT32_MAX, &events, console.crPending ? pdMS_TO_TICKS(1U) : portMAX_DELAY);
        }
    }
}
