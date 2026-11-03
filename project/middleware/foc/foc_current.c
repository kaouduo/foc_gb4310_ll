/**
 * @file foc_current.c
 * @brief 电流斜坡、联合限幅与回算抗积分饱和。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_current.h"
#include <math.h>
#include "foc_math.h"
#include "ramp.h"
/** @brief 检查电流模式所需的有效反馈和标定。 */
static bool Ready(const FOC_Input *in)
{
    return in->encoder_valid && in->current_valid && in->calibration_valid && isfinite(in->electrical_angle_rad) &&
            isfinite(in->id_a) && isfinite(in->iq_a);
}
/** @brief 进入电流模式并清零积分和电流斜坡。 */
static Sys_Status Enter(FOC *base, const FOC_Input *in)
{
    FOC_Current *self = (FOC_Current *)base;
    self->entered = false;
    if (!self->configured || !Ready(in)) {
        return SYS_ERROR_NOT_READY;
    }
    PI_Reset(&self->d_pi, 0.0f);
    PI_Reset(&self->q_pi, 0.0f);
    self->applied_d_a = self->applied_q_a = 0.0f;
    self->entered = true;
    return SYS_OK;
}
/** @brief 计算电流斜坡、双 PI、联合限幅和积分回算。 */
static Sys_Status Step(FOC *base, const FOC_Input *in, FOC_Output *out)
{
    FOC_Current *self = (FOC_Current *)base;
    if (!self->entered || !Ready(in)) {
        return SYS_ERROR_NOT_READY;
    }
    float rd = self->command.id_a, rq = self->command.iq_a;
    Sys_Status status = FOC_LimitVector(&rd, &rq, self->current_limit_a);
    if (status != SYS_OK) {
        return status;
    }
    Ramp_Vector(&self->applied_d_a, &self->applied_q_a, rd, rq, self->command.ramp_a_per_s * in->dt_s);
    float d = PI_Step(&self->d_pi, self->applied_d_a - in->id_a, in->dt_s);
    float q = PI_Step(&self->q_pi, self->applied_q_a - in->iq_a, in->dt_s);
    float applied_d = d, applied_q = q;
    status = FOC_LimitVector(&applied_d, &applied_q, in->voltage_limit_v);
    if (status != SYS_OK) {
        return status;
    }
    PI_Track(&self->d_pi, d, applied_d, in->dt_s);
    PI_Track(&self->q_pi, q, applied_q, in->dt_s);
    if (!isfinite(self->d_pi.integral) || !isfinite(self->q_pi.integral)) {
        return SYS_ERROR_PARAM;
    }
    *out = (FOC_Output){FOC_WrapAngle(in->electrical_angle_rad), applied_d, applied_q};
    return SYS_OK;
}
/** @brief 退出电流模式并清零积分和斜坡。 */
static void Exit(FOC *base)
{
    FOC_Current *self = (FOC_Current *)base;
    PI_Reset(&self->d_pi, 0.0f);
    PI_Reset(&self->q_pi, 0.0f);
    self->applied_d_a = self->applied_q_a = 0.0f;
    self->entered = false;
}
static const FOC_Ops ops = {Enter, Step, Exit};
/** @brief 构造电流模式，不操作硬件。 */
void FOC_Current_Bind(FOC_Current *self)
{
    *self = (FOC_Current){.command = {0.0f, 0.0f, 1.0f}};
    FOC_Base_Bind(&self->base, &ops);
}
/** @brief 停机设置两轴 PI 和电流指令幅值上限。 */
Sys_Status FOC_Current_Configure(FOC_Current *self, const PI_Controller *d, const PI_Controller *q, float limit)
{
    if (self->entered) {
        return SYS_ERROR_BUSY;
    }
    if (!PI_Valid(d) || !PI_Valid(q) || d->kp <= 0.0f || q->kp <= 0.0f || !isfinite(limit) || limit <= 0.0f) {
        return SYS_ERROR_PARAM;
    }
    self->d_pi = *d;
    self->q_pi = *q;
    PI_Reset(&self->d_pi, 0.0f);
    PI_Reset(&self->q_pi, 0.0f);
    self->current_limit_a = limit;
    self->configured = true;
    return SYS_OK;
}
/** @brief 更新电流命令。 */
Sys_Status FOC_Current_SetCommand(FOC_Current *self, const FOC_CurrentCommand *cmd)
{
    if (!isfinite(cmd->id_a) || !isfinite(cmd->iq_a) || !isfinite(cmd->ramp_a_per_s) || cmd->ramp_a_per_s <= 0.0f ||
        !isfinite(hypotf(cmd->id_a, cmd->iq_a))) {
        return SYS_ERROR_PARAM;
    }
    self->command = *cmd;
    return SYS_OK;
}
