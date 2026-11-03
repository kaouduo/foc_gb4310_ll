/**
 * @file app_shell.c
 * @brief 管理 CherrySH 会话与有界终端输入，不访问硬件。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "app_shell.h"
#include <string.h>
#include "app_motor.h"
#include "csh.h"

static chry_shell_t shell;
static char prompt[64];
static _Alignas(2) char history[256];
/* 上游行缓冲头为 uint16_t，历史读写按双字节对齐。 */
static _Alignas(2) char line[128];
static App_ShellWrite writeOutput;
static uint8_t key[4], keyLength, keyOffset, escapeLength;
static bool suppressLF, overflow, clearRequested, outputFailed;
static App_USBAction pendingAction;
extern const chry_syscall_t __fsymtab_start[], __fsymtab_end[];
extern const chry_sysvar_t __vsymtab_start[], __vsymtab_end[];
static const char path[] = "/bin:/sbin";
CSH_RVAR_EXPORT(path, PATH, sizeof(path));

static uint8_t (*defaultCompletion)(chry_readline_t *, char *, uint16_t *, const char **, uint8_t *, uint8_t);

/* 保留上游命令/变量补全，为项目命令的二、三级参数提供候选。 */
static uint8_t Complete(chry_readline_t *rl, char *pre, uint16_t *size, const char **argv, uint8_t *argl,
                        uint8_t maximum)
{
    const char *end = rl->ln.buff->pbuf + rl->ln.curoff;
    const char *cursor = rl->ln.buff->pbuf;
    const char *words[3];
    uint16_t lengths[3];
    uint8_t wordCount = 0U;
    while (cursor < end) {
        while (cursor < end && *cursor == ' ') {
            ++cursor;
        }
        if (wordCount == 3U) {
            return 0U;
        }
        words[wordCount] = cursor;
        while (cursor < end && *cursor != ' ') {
            ++cursor;
        }
        lengths[wordCount] = (uint16_t)(cursor - words[wordCount]);
        ++wordCount;
    }
    if (wordCount <= 1U) {
        return defaultCompletion(rl, pre, size, argv, argl, maximum);
    }
    static const char *const system[] = {"uid", "time"};
    static const char *const test[] = {
            "enc",  "bus",   "current", "zero", "motor", "stop",
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
            "sync", "chain", "foc",     "pwm",
#endif
    };
    static const char *const motorCommands[] = {"open", "stop", "status", "clear", "cal"};
    static const char *const calibrationCommands[] = {"start", "status", "stop"};
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    static const char *const phases[] = {"u", "v", "w", "all"};
#endif
    const char *const *candidates = NULL;
    uint8_t count = 0U;
    if (wordCount == 2U && lengths[0] == 6U && memcmp(words[0], "system", 6U) == 0) {
        candidates = system;
        count = 2U;
    } else if (wordCount == 2U && lengths[0] == 4U && memcmp(words[0], "test", 4U) == 0) {
        candidates = test;
        count = sizeof(test) / sizeof(test[0]);
    } else if (wordCount == 2U && lengths[0] == 5U && memcmp(words[0], "motor", 5U) == 0) {
        candidates = motorCommands;
        count = sizeof(motorCommands) / sizeof(motorCommands[0]);
    } else if (wordCount == 3U && lengths[0] == 5U && memcmp(words[0], "motor", 5U) == 0 && lengths[1] == 3U &&
               memcmp(words[1], "cal", 3U) == 0) {
        candidates = calibrationCommands;
        count = sizeof(calibrationCommands) / sizeof(calibrationCommands[0]);
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    } else if (wordCount == 3U && lengths[0] == 4U && memcmp(words[0], "test", 4U) == 0 && lengths[1] == 3U &&
               memcmp(words[1], "pwm", 3U) == 0) {
        candidates = phases;
        count = 4U;
#endif
    }
#if defined(FOC_LOG_DEBUG_BUILD)
    static const char *const logCommands[] = {"level", "color", "stats"};
    static const char *const levels[] = {"off", "error", "warn", "info", "debug"};
    static const char *const colors[] = {"on", "off"};
    if (lengths[0] == 3U && memcmp(words[0], "log", 3U) == 0) {
        if (wordCount == 2U) {
            candidates = logCommands;
            count = 3U;
        } else if (lengths[1] == 5U && memcmp(words[1], "level", 5U) == 0) {
            candidates = levels;
            count = 5U;
        } else if (lengths[1] == 5U && memcmp(words[1], "color", 5U) == 0) {
            candidates = colors;
            count = 2U;
        }
    }
#endif
    uint8_t matches = 0U;
    for (uint8_t i = 0U; i < count && matches < maximum; ++i) {
        size_t length = strlen(candidates[i]);
        if (*size <= length && memcmp(pre, candidates[i], *size) == 0) {
            argv[matches] = candidates[i];
            argl[matches++] = (uint8_t)length;
        }
    }
    return matches;
}

static uint16_t Write(chry_readline_t *rl, const void *data, uint16_t length)
{
    (void)rl;
    if (outputFailed) {
        return 0U;
    }
    uint16_t completed = writeOutput(data, length);
    if (completed != length) {
        outputFailed = true;
    }
    return completed;
}

/* 只向上游暴露完整按键，避免非阻塞模式内部的 ESC 等待循环。 */
static uint16_t Read(chry_readline_t *rl, void *data, uint16_t length)
{
    (void)rl;
    (void)length;
    if (keyOffset == keyLength || outputFailed) {
        return 0U;
    }
    *(uint8_t *)data = key[keyOffset++];
    return 1U;
}

static Sys_Status BuildSession(void)
{
    memset(&shell, 0, sizeof(shell));
    memset(history, 0, sizeof(history));
    memset(line, 0, sizeof(line));
    const chry_shell_init_t init = {
            .sput = Write,
            .sget = Read,
            .command_table_beg = __fsymtab_start,
            .command_table_end = __fsymtab_end,
            .variable_table_beg = __vsymtab_start,
            .variable_table_end = __vsymtab_end,
            .prompt_buffer = prompt,
            .prompt_buffer_size = sizeof(prompt),
            .history_buffer = history,
            .history_buffer_size = sizeof(history),
            .line_buffer = line,
            .line_buffer_size = sizeof(line),
            .uid = 0,
            .host = "",
            .user = {"root"},
            .hash = {""},
    };
    if (chry_shell_init(&shell, &init) != 0) {
        return SYS_ERROR_PARAM;
    }
    chry_readline_prompt_clear(&shell.rl);
    defaultCompletion = shell.rl.cplt.acb;
    chry_readline_set_completion_cb(&shell.rl, Complete);
    uint16_t style = (chry_readline_sgr_t){.foreground = CHRY_READLINE_SGR_GREEN, .bold = 1}.raw;
    int result = chry_readline_prompt_edit(&shell.rl, 0U, style, "root");
    result |= chry_readline_prompt_edit(&shell.rl, 1U, 0U, "> ");
    return result == 0 ? SYS_OK : SYS_ERROR_PARAM;
}

static void ShowPrompt(void)
{
    keyLength = keyOffset = 0U;
    if (chry_shell_task_repl(&shell) < 0) {
        outputFailed = true;
    }
}

Sys_Status App_Shell_Open(App_ShellWrite write)
{
    writeOutput = write;
    keyLength = keyOffset = escapeLength = 0U;
    suppressLF = overflow = clearRequested = outputFailed = false;
    pendingAction = APP_USB_ACTION_NONE;
    Sys_Status status = BuildSession();
    if (status != SYS_OK) {
        return status;
    }
    ShowPrompt();
    return outputFailed ? SYS_ERROR_IO : SYS_OK;
}

static void DispatchKey(uint8_t length)
{
    keyLength = length;
    keyOffset = 0U;
    int result = chry_shell_task_repl(&shell);
    if (result < 0) {
        outputFailed = true;
    }
    if (outputFailed || pendingAction != APP_USB_ACTION_NONE) {
        return;
    }
    if (clearRequested) {
        clearRequested = false;
        if (BuildSession() != SYS_OK) {
            outputFailed = true;
            return;
        }
        static const char clear[] = "\033[2J\033[H";
        (void)Write(&shell.rl, clear, sizeof(clear) - 1U);
        ShowPrompt();
    } else if (result == 0) {
        ShowPrompt();
    }
}

static void EscapeByte(uint8_t byte)
{
    static const char *const sequences[] = {
            "\033[A",  "\033[B",  "\033[C",  "\033[D", "\033[H", "\033[F", "\033[1~", "\033[3~",
            "\033[4~", "\033[7~", "\033[8~", "\033OH", "\033OF", "\033b",  "\033f",
    };
    key[escapeLength++] = byte;
    for (uint32_t i = 0U; i < sizeof(sequences) / sizeof(sequences[0]); ++i) {
        size_t length = strlen(sequences[i]);
        if (escapeLength <= length && memcmp(key, sequences[i], escapeLength) == 0) {
            if (escapeLength == length) {
                escapeLength = 0U;
                DispatchKey((uint8_t)length);
            }
            return;
        }
    }
    escapeLength = 0U;
}

void App_Shell_Feed(uint8_t byte)
{
    if (outputFailed || pendingAction != APP_USB_ACTION_NONE) {
        return;
    }
    if (suppressLF) {
        suppressLF = false;
        if (byte == '\n') {
            return;
        }
    }
    if (byte == '\r' || byte == '\n') {
        suppressLF = byte == '\r';
        escapeLength = 0U;
        if (overflow) {
            overflow = false;
            (void)chry_readline_edit_delline(&shell.rl);
            static const char error[] = "\r\nERR line too long\r\n";
            (void)Write(&shell.rl, error, sizeof(error) - 1U);
        }
        key[0] = '\n';
        DispatchKey(1U);
    } else if (overflow) {
        return;
    } else if (escapeLength != 0U) {
        EscapeByte(byte);
    } else if (byte == '\t' && shell.rl.ln.curoff != 0U && shell.rl.ln.buff->pbuf[shell.rl.ln.curoff - 1U] == ' ') {
        /* 上游将空单词的 Tab 作为缩进；管理台在这里改为列出参数候选。 */
        int result = chry_readline_complete(&shell.rl);
        if (result < 0) {
            outputFailed = true;
        } else if (result > 0) {
            App_Shell_AfterLog();
        }
    } else if (byte == 0x1BU) {
        key[0] = byte;
        escapeLength = 1U;
    } else if (byte != 0U && byte < 0x80U && byte != 0x0FU) {
        if (byte >= 0x20U && byte < 0x7FU && shell.rl.ln.buff->size == shell.rl.ln.lnmax) {
            overflow = true;
            return;
        }
        key[0] = byte;
        DispatchKey(1U);
    }
}

bool App_Shell_OutputFailed(void)
{
    return outputFailed;
}
App_USBAction App_Shell_PendingAction(void)
{
    return pendingAction;
}
void App_Shell_RequestClear(void)
{
    clearRequested = true;
}
void App_Shell_RequestAction(App_USBAction action)
{
    pendingAction = action;
}

/* 不修改行缓冲、历史和 escape 分包状态，只操作终端显示。 */
bool App_Shell_BeforeLog(void)
{
    static const char clearLine[] = "\r\033[2K";
    (void)Write(&shell.rl, clearLine, sizeof(clearLine) - 1U);
    return !outputFailed;
}
void App_Shell_AfterLog(void)
{
    (void)Write(&shell.rl, shell.rl.prompt, shell.rl.ln.pptlen);
    (void)chry_readline_edit_refresh(&shell.rl);
}
