/**
 * @file app_motor_task.c
 * @brief 电机 1 ms 监督任务，快速环仍由 ADC IRQ 执行。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include "FreeRTOS.h"
#include "app_motor.h"
#include "task.h"
/** @brief 每 1 ms 调用电机监督流程。 */
void App_MotorTask(void *argument)
{
    (void)argument;
    TickType_t previous = xTaskGetTickCount();
    for (;;) {
        App_Motor_Process();
        TickType_t now = xTaskGetTickCount();
        if (now - previous > pdMS_TO_TICKS(2U)) {
            previous = now;
        }
        vTaskDelayUntil(&previous, pdMS_TO_TICKS(1U));
    }
}
