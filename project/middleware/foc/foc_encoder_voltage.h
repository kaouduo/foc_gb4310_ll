/**
 * @file foc_encoder_voltage.h
 * @brief 编码器角度电压模式。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_ENCODER_VOLTAGE_H
#define FOC_ENCODER_VOLTAGE_H

#include "foc_base.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float ud_v;   /**< d 轴目标电压，单位 V。 */
    float uq_v;   /**< q 轴目标电压，单位 V。 */
    float ramp_s; /**< dq 矢量到达新目标的斜坡时间，单位 s，必须为正。 */
} FOC_EncoderVoltageCommand;
typedef struct {
    FOC base; /**< 首成员基类。 */
    FOC_EncoderVoltageCommand command;
    float applied_d_v;       /**< 当前 d 轴电压，单位 V。 */
    float applied_q_v;       /**< 当前 q 轴电压，单位 V。 */
    float ramp_rate_v_per_s; /**< dq 矢量斜坡速率，单位 V/s。 */
    bool entered;            /**< 是否已成功进入模式。 */
} FOC_EncoderVoltage;
/**
 * @brief 构造编码器电压模式，不操作硬件。
 * @param self 调用方持有的对象。
 * @return 无。
 * @note 对象唯一拥有者构造，进入模式不等于允许功率输出。
 */
void FOC_EncoderVoltage_Bind(FOC_EncoderVoltage *self);
/**
 * @brief 更新编码器电压命令。
 * @param self 模式对象。
 * @param command 有限目标：dq 电压 V、正斜坡时间 s。
 * @return SYS_OK 或 PARAM；非法命令不覆盖原命令。
 * @note 仅对象唯一拥有者在周期边界调用，不与 step 并发。
 */
Sys_Status FOC_EncoderVoltage_SetCommand(FOC_EncoderVoltage *self, const FOC_EncoderVoltageCommand *command);

#ifdef __cplusplus
}
#endif
#endif
