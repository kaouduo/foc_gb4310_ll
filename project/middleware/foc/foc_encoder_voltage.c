/**
 * @file foc_encoder_voltage.c
 * @brief 编码器反馈检查与 dq 电压斜坡。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_encoder_voltage.h"
#include <math.h>
#include "foc_math.h"
#include "ramp.h"
/** @brief 检查编码器、标定及电角度有效性。 */
static bool Ready(const FOC_Input *in)
{
    return in->encoder_valid && in->calibration_valid && isfinite(in->electrical_angle_rad);
}
/** @brief 进入编码器电压模式并复位斜坡。 */
static Sys_Status Enter(FOC *base, const FOC_Input *input)
{
    FOC_EncoderVoltage *self = (FOC_EncoderVoltage *)base;
    self->entered = false;
    if (!Ready(input)) {
        return SYS_ERROR_NOT_READY;
    }
    self->applied_d_v = self->applied_q_v = 0.0f;
    self->ramp_rate_v_per_s = hypotf(self->command.ud_v, self->command.uq_v) / self->command.ramp_s;
    self->entered = true;
    return SYS_OK;
}
/** @brief 按反馈角度生成受动态上限约束的 dq 电压。 */
static Sys_Status Step(FOC *base, const FOC_Input *in, FOC_Output *out)
{
    FOC_EncoderVoltage *self = (FOC_EncoderVoltage *)base;
    if (!self->entered || !Ready(in)) {
        return SYS_ERROR_NOT_READY;
    }
    float d = self->command.ud_v, q = self->command.uq_v;
    Sys_Status status = FOC_LimitVector(&d, &q, in->voltage_limit_v);
    if (status != SYS_OK) {
        return status;
    }
    Ramp_Vector(&self->applied_d_v, &self->applied_q_v, d, q, self->ramp_rate_v_per_s * in->dt_s);
    status = FOC_LimitVector(&self->applied_d_v, &self->applied_q_v, in->voltage_limit_v);
    *out = (FOC_Output){FOC_WrapAngle(in->electrical_angle_rad), self->applied_d_v, self->applied_q_v};
    return status;
}
/** @brief 退出模式并清零已施加的电压。 */
static void Exit(FOC *base)
{
    FOC_EncoderVoltage *self = (FOC_EncoderVoltage *)base;
    self->entered = false;
    self->applied_d_v = self->applied_q_v = 0.0f;
}
static const FOC_Ops ops = {Enter, Step, Exit};
/** @brief 构造编码器电压模式，不操作硬件。 */
void FOC_EncoderVoltage_Bind(FOC_EncoderVoltage *self)
{
    *self = (FOC_EncoderVoltage){.command = {0.0f, 0.0f, 0.2f}};
    FOC_Base_Bind(&self->base, &ops);
}
/** @brief 更新编码器电压命令。 */
Sys_Status FOC_EncoderVoltage_SetCommand(FOC_EncoderVoltage *self, const FOC_EncoderVoltageCommand *cmd)
{
    if (!isfinite(cmd->ud_v) || !isfinite(cmd->uq_v) || !isfinite(cmd->ramp_s) || cmd->ramp_s <= 0.0f ||
        !isfinite(hypotf(cmd->ud_v, cmd->uq_v))) {
        return SYS_ERROR_PARAM;
    }
    self->ramp_rate_v_per_s = hypotf(cmd->ud_v - self->applied_d_v, cmd->uq_v - self->applied_q_v) / cmd->ramp_s;
    self->command = *cmd;
    return SYS_OK;
}
