/** @file elog_port.c @brief 无硬件依赖的 EasyLogger 静态互斥与回调适配。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "elog_port.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "elog.h"
#include "semphr.h"
#include "task.h"
static StaticSemaphore_t lockStorage;
static SemaphoreHandle_t lock;
static bool (*sink)(const char *, size_t);
static uint32_t (*clockMs)(void);
void Elog_Port_Bind(bool (*output)(const char *, size_t), uint32_t (*timeMs)(void))
{
    sink = output;
    clockMs = timeMs;
}
bool Elog_Port_InISR(void) { return __get_IPSR() != 0U; }
ElogErrCode elog_port_init(void)
{
    lock = xSemaphoreCreateMutexStatic(&lockStorage);
    return lock != NULL ? ELOG_NO_ERR : (ElogErrCode)1;
}
ElogErrCode elog_port_deinit(void) { return ELOG_NO_ERR; }
void elog_port_output_lock(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        (void)xSemaphoreTake(lock, portMAX_DELAY);
    }
}
void elog_port_output_unlock(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        (void)xSemaphoreGive(lock);
    }
}
void elog_port_output(const char *log, size_t length)
{
    if (sink != NULL) {
        (void)sink(log, length);
    }
}
const char *elog_port_get_time(void)
{
    static char value[16];
    uint32_t ms = clockMs != NULL ? clockMs() : 0U;
    (void)snprintf(value, sizeof(value), "%lu.%03lu", (unsigned long)(ms / 1000U), (unsigned long)(ms % 1000U));
    return value;
}
const char *elog_port_get_p_info(void) { return ""; }
const char *elog_port_get_t_info(void) { return ""; }
