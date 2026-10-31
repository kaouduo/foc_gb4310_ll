/** @file app_log.c @brief 项目格式、运行时过滤和统计；不等待 CDC。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "app_log.h"
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include "app_usb_task.h"
#include "elog.h"
static const App_LogPlatform *platform;
static atomic_bool ready;
#if defined(FOC_LOG_DEBUG_BUILD)
static atomic_uint threshold = APP_LOG_DEBUG;
#else
static atomic_uint threshold = APP_LOG_INFO;
#endif
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "Log counters require lock-free word atomics");
static atomic_uint filtered;
static uint32_t emitted, truncated;
static const uint8_t levels[] = {0U, ELOG_LVL_ERROR, ELOG_LVL_WARN, ELOG_LVL_INFO, ELOG_LVL_DEBUG};
bool App_Log_Init(const App_LogPlatform *binding)
{
    if (binding == NULL || binding->lock == NULL || binding->unlock == NULL || binding->inISR == NULL) {
        return false;
    }
    platform = binding;
    if (elog_init() != ELOG_NO_ERR) {
        return false;
    }
    /* 所有库访问由本门面持有同一把锁，避免配置与格式化并发。 */
    elog_output_lock_enabled(false);
    for (uint8_t i = 0U; i <= ELOG_LVL_VERBOSE; ++i) {
        elog_set_fmt(i, ELOG_FMT_TIME);
    }
    elog_set_filter_lvl(ELOG_OUTPUT_LVL);
    elog_set_output_enabled(true);
    atomic_store(&ready, true);
    return true;
}
bool App_Log_Enabled(App_LogLevel level)
{
    bool enabled = atomic_load(&ready) && level >= APP_LOG_ERROR && level <= APP_LOG_DEBUG &&
            (unsigned)level <= atomic_load(&threshold) && !platform->inISR();
    if (!enabled) {
        (void)atomic_fetch_add(&filtered, 1U);
    }
    return enabled;
}
void App_Log_Write(App_LogLevel level, const char *tag, const char *format, ...)
{
    if (!atomic_load(&ready) || platform->inISR() || level < APP_LOG_ERROR || level > APP_LOG_DEBUG) {
        return;
    }
    platform->lock();
    if ((unsigned)level > atomic_load(&threshold)) {
        (void)atomic_fetch_add(&filtered, 1U);
        platform->unlock();
        return;
    }
    char message[144];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (count < 0) {
        message[0] = '\0';
    }
    if (count >= (int)sizeof(message)) {
        ++truncated;
        message[sizeof(message) - 4] = '.';
        message[sizeof(message) - 3] = '.';
        message[sizeof(message) - 2] = '.';
    }
    elog_output(levels[level], tag, NULL, NULL, 0L, "%c %.16s: %s", "?EWID"[level], tag, message);
    ++emitted;
    platform->unlock();
}
void App_Log_SetLevel(App_LogLevel level)
{
    if (level > APP_LOG_DEBUG) {
        return;
    }
#ifndef FOC_LOG_DEBUG_BUILD
    if (level > APP_LOG_INFO) {
        level = APP_LOG_INFO;
    }
#endif
    platform->lock();
    atomic_store(&threshold, (unsigned)level);
    platform->unlock();
}
void App_Log_SetColor(bool enabled)
{
    platform->lock();
    elog_set_text_color_enabled(enabled);
    platform->unlock();
}
App_LogStats App_Log_GetStats(void)
{
    platform->lock();
    App_LogStats result = {.emitted = emitted,
                           .filtered = atomic_load(&filtered),
                           .truncated = truncated,
                           .dropped = App_USBOutput_DroppedLogs(),
                           .level = (App_LogLevel)atomic_load(&threshold),
                           .color = elog_get_text_color_enabled()};
    platform->unlock();
    return result;
}
