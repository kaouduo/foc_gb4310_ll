/**
 *  @file   app_usb_console.c
 *  @brief  解析文本行、生成回显及 UID/复位/DFU 响应。
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
#include "app_usb_console.h"
#include <string.h>
#include "bsp_platform.h"

static void Respond(App_USBConsole *console, const char *text)
{
    console->responseLength = (uint32_t)strlen(text);
    memcpy(console->response, text, console->responseLength);
}

static void Finish(App_USBConsole *console, const char *ending, uint32_t endingLength)
{
    if (console->overflow) {
        Respond(console, "ERR line too long\r\n");
    } else if (console->length == 3U && memcmp(console->line, "uid", 3U) == 0) {
        uint32_t uid[3];
        static const char hex[] = "0123456789ABCDEF";
        (void)BSP_Platform_UniqueId(uid);
        memcpy(console->response, "UID=", 4U);
        for (uint32_t word = 0U; word < 3U; ++word) {
            for (uint32_t digit = 0U; digit < 8U; ++digit) {
                console->response[4U + word * 8U + digit] = hex[(uid[word] >> (28U - digit * 4U)) & 15U];
            }
        }
        memcpy(console->response + 28U, "\r\n", 2U);
        console->responseLength = 30U;
    } else if (console->length == 6U && memcmp(console->line, "reboot", 6U) == 0) {
        Respond(console, "OK reboot\r\n");
        console->action = APP_USB_ACTION_REBOOT;
    } else if (console->length == 3U && memcmp(console->line, "dfu", 3U) == 0) {
        Respond(console, "OK dfu\r\n");
        console->action = APP_USB_ACTION_DFU;
    } else {
        memcpy(console->response, console->line, console->length);
        memcpy(console->response + console->length, ending, endingLength);
        console->responseLength = console->length + endingLength;
    }
    console->length = 0U;
    console->overflow = false;
    console->crPending = false;
}

void App_USBConsole_Reset(App_USBConsole *console) { memset(console, 0, sizeof(*console)); }
void App_USBConsole_Consumed(App_USBConsole *console) { console->responseLength = 0U; }
void App_USBConsole_Poll(App_USBConsole *console, uint32_t nowMs)
{
    if (console->crPending && nowMs - console->crTime >= 10U) {
        Finish(console, "\r", 1U);
        console->suppressLF = true;
    }
}

bool App_USBConsole_Feed(App_USBConsole *console, uint8_t byte, uint32_t nowMs)
{
    if (console->responseLength != 0U) {
        return false;
    }
    if (console->suppressLF) {
        console->suppressLF = false;
        if (byte == '\n') {
            return true;
        }
    }
    if (console->crPending) {
        Finish(console, byte == '\n' ? "\r\n" : "\r", byte == '\n' ? 2U : 1U);
        return byte == '\n';
    }
    if (byte == '\r') {
        console->crPending = true;
        console->crTime = nowMs;
    } else if (byte == '\n') {
        Finish(console, "\n", 1U);
    } else if (console->length < 128U) {
        console->line[console->length++] = byte;
    } else {
        console->overflow = true;
    }
    return true;
}
