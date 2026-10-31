/** @file app_log.h @brief 普通任务和调度前日志入口；禁止 ISR/异常上下文。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef APP_LOG_H
#define APP_LOG_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { APP_LOG_OFF, APP_LOG_ERROR, APP_LOG_WARN, APP_LOG_INFO, APP_LOG_DEBUG } App_LogLevel;
typedef struct {
    uint32_t emitted, filtered, truncated, dropped;
    App_LogLevel level;
    bool color;
} App_LogStats;
/* 由 main 注入静态生命周期的平台能力，业务日志不依赖 porting。 */
typedef struct {
    void (*lock)(void);
    void (*unlock)(void);
    bool (*inISR)(void);
} App_LogPlatform;
/* main 在平台时间源可用后、任何任务创建前调用一次。 */
bool App_Log_Init(const App_LogPlatform *platform);
bool App_Log_Enabled(App_LogLevel level);
void App_Log_Write(App_LogLevel level, const char *tag, const char *format, ...)
#if defined(__GNUC__)
        __attribute__((format(printf, 3, 4)))
#endif
        ;
void App_Log_SetLevel(App_LogLevel level);
void App_Log_SetColor(bool enabled);
App_LogStats App_Log_GetStats(void);
#define APP_LOG_AT(level, tag, ...)                                                                                    \
    do {                                                                                                               \
        if (App_Log_Enabled(level)) {                                                                                  \
            App_Log_Write(level, tag, __VA_ARGS__);                                                                    \
        }                                                                                                              \
    } while (0)
#define LOG_E(tag, ...) APP_LOG_AT(APP_LOG_ERROR, tag, __VA_ARGS__)
#define LOG_W(tag, ...) APP_LOG_AT(APP_LOG_WARN, tag, __VA_ARGS__)
#define LOG_I(tag, ...) APP_LOG_AT(APP_LOG_INFO, tag, __VA_ARGS__)
#if defined(FOC_LOG_DEBUG_BUILD)
#define LOG_D(tag, ...) APP_LOG_AT(APP_LOG_DEBUG, tag, __VA_ARGS__)
#else
#define LOG_D(...) ((void)0)
#endif
#ifdef __cplusplus
}
#endif
#endif
