/**
 * @file app_motor_telemetry.h
 * @brief 成功控制周期的最新快照；单 IRQ 写、单任务读。
 * @author kaouduo
 * @date 2026/11/02
 */
#ifndef APP_MOTOR_TELEMETRY_H
#define APP_MOTOR_TELEMETRY_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define APP_MOTOR_TELEMETRY_CHANNELS 16U
#define APP_MOTOR_TELEMETRY_DIVIDER 200U /**< 20 kHz 环每 200 帧发布，最高 100 Hz。 */
typedef struct {
    float channels[APP_MOTOR_TELEMETRY_CHANNELS]; /**< 顺序/单位见 18_vofa_uart_dma.md。 */
    uint32_t sequence;                            /**< 每次发布递增，允许回绕；任务不重复发送旧序号。 */
} App_MotorTelemetry;
/** @brief 任务短临界区复制同周期数据；无成功控制数据时返回 false。
 * @param out 调用者拥有的完整副本，不保留指针。
 * @note 新鲜度由调用者比较序号，不把停机旧值当作新数据。
 */
bool App_Motor_ReadTelemetry(App_MotorTelemetry *out);
#ifdef __cplusplus
}
#endif
#endif
