/**
 * @file app_vofa_task.c
 * @brief 低优先级最新快照组帧及 USART3 DMA 提交，忙时丢帧不等待。
 * @author kaouduo
 * @date 2026/11/02
 */
#include "FreeRTOS.h"
#include "app_motor_telemetry.h"
#include "app_task.h"
#include "bsp_vofa.h"
#include "task.h"
#include "vofa_justfloat.h"
/** @brief 唯一任务写入，仅供调试器观察；submitted 为 DMA 接受数。 */
volatile struct {
    uint32_t submitted;
    uint32_t skipped;
    uint32_t busy;
    uint32_t errors;
} appVofaStats;
void App_VOFA_Task(void *argument)
{
    (void)argument;
    uint32_t previous_sequence = 0U;
    TickType_t previous = xTaskGetTickCount();
    App_MotorTelemetry sample;
    uint8_t frame[VOFA_JUSTFLOAT_FRAME_BYTES(APP_MOTOR_TELEMETRY_CHANNELS)];
    for (;;) {
        if (App_Motor_ReadTelemetry(&sample) && sample.sequence != previous_sequence) {
            appVofaStats.skipped += sample.sequence - previous_sequence - 1U;
            previous_sequence = sample.sequence;
            uint16_t length = VOFA_JustFloat_Build(sample.channels, APP_MOTOR_TELEMETRY_CHANNELS, frame, sizeof(frame));
            Sys_Status status = BSP_VOFA.write(BSP_VOFA.context, frame, length);
            if (status == SYS_OK) {
                ++appVofaStats.submitted;
            } else if (status == SYS_ERROR_BUSY) {
                ++appVofaStats.busy;
            } else {
                ++appVofaStats.errors;
            }
        }
        TickType_t now = xTaskGetTickCount();
        if (now - previous > pdMS_TO_TICKS(2U)) {
            previous = now;
        }
        vTaskDelayUntil(&previous, pdMS_TO_TICKS(1U));
    }
}
