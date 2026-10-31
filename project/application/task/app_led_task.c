/**
 *  @file   app_led_task.c
 *  @brief  周期更新红灯指示而不参与电机控制。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  application / task
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 增加红灯硬件呼吸指示与独立整机阶段、锁存故障模型。
 */

#include "FreeRTOS.h"
#include "app_device_state.h"
#include "app_led.h"
#include "app_log.h"
#include "app_task.h"
#include "bsp_led.h"
#include "task.h"

/**
 *  @brief  周期读取整机状态并更新红灯亮度。
 *  @param  argument 预留任务参数，当前不使用，可为空。
 *  @return 不返回，持续运行指示任务。
 *  @note   调度器运行后且红灯已绑定时执行；每 10 ms 更新亮度，其余时间阻塞；不复位、清故障或控制电机。
 */
void App_LEDTask(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    TickType_t epoch = wake;
    System_State previous = App_DeviceState_Get();
    while (1) {
        System_State state = App_DeviceState_Get();
        TickType_t now = xTaskGetTickCount();
        if (state.state != previous.state) {
            epoch = now;
        }
        if (state.state != previous.state || state.faults != previous.faults) {
            if (state.faults != 0U) {
                LOG_E("system", "state=%u faults=0x%08lX", (unsigned)state.state, (unsigned long)state.faults);
            } else {
                LOG_I("system", "state=%u", (unsigned)state.state);
            }
            previous = state;
        }
        uint32_t elapsedMs = (uint32_t)((uint64_t)(TickType_t)(now - epoch) * 1000U / configTICK_RATE_HZ);
        LED_SetBrightness(BSP_LED_Red, App_LED_Brightness(state, elapsedMs));
        (void)xTaskDelayUntil(&wake, pdMS_TO_TICKS(10U));
    }
}
