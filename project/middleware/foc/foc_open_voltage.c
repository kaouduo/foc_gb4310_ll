/**
 * @file foc_open_voltage.c
 * @brief 角度积分与电压斜坡，不管理试验总时间。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_open_voltage.h"
#include <math.h>
#include "foc_math.h"
#include "ramp.h"
/** @brief 复位积分角度和电压斜坡，进入开环模式。 */
static Sys_Status Enter(FOC *base, const FOC_Input *input)
{
    (void)input;
    FOC_OpenVoltage *self = (FOC_OpenVoltage *)base;
    self->angle_rad = self->applied_v = 0.0f;
    self->ramp_rate_v_per_s = self->command.voltage_v / self->command.ramp_s;
    self->entered = true;
    return SYS_OK;
}
/** @brief 积分电角度并按斜坡和动态上限更新幅值。 */
static Sys_Status Step(FOC *base, const FOC_Input *input, FOC_Output *out)
{
    FOC_OpenVoltage *self = (FOC_OpenVoltage *)base;
    if (!self->entered) {
        return SYS_ERROR_NOT_READY;
    }
    float target = self->command.voltage_v < input->voltage_limit_v ? self->command.voltage_v : input->voltage_limit_v;
    self->applied_v = Ramp_Step(self->applied_v, target, self->ramp_rate_v_per_s * input->dt_s);
    if (self->applied_v > input->voltage_limit_v) {
        self->applied_v = input->voltage_limit_v;
    }
    self->angle_rad = FOC_WrapAngle(self->angle_rad + FOC_TWO_PI * self->command.frequency_hz * input->dt_s);
    *out = (FOC_Output){self->angle_rad, self->applied_v, 0.0f};
    return SYS_OK;
}
/** @brief 退出开环并清零角度与幅值。 */
static void Exit(FOC *base)
{
    FOC_OpenVoltage *self = (FOC_OpenVoltage *)base;
    self->entered = false;
    self->applied_v = self->angle_rad = 0.0f;
}
static const FOC_Ops ops = {Enter, Step, Exit};
/** @brief 构造开环电压模式，不操作硬件。 */
void FOC_OpenVoltage_Bind(FOC_OpenVoltage *self)
{
    *self = (FOC_OpenVoltage){.command = {2.0f, 1.0f, 0.2f}};
    FOC_Base_Bind(&self->base, &ops);
}
/** @brief 更新开环电压命令。 */
Sys_Status FOC_OpenVoltage_SetCommand(FOC_OpenVoltage *self, const FOC_OpenVoltageCommand *cmd)
{
    if (!isfinite(cmd->frequency_hz) || !isfinite(cmd->voltage_v) || !isfinite(cmd->ramp_s) || cmd->voltage_v < 0.0f ||
        cmd->ramp_s <= 0.0f) {
        return SYS_ERROR_PARAM;
    }
    self->ramp_rate_v_per_s = fabsf(cmd->voltage_v - self->applied_v) / cmd->ramp_s;
    self->command = *cmd;
    return SYS_OK;
}
