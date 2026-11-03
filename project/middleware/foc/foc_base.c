/**
 * @file foc_base.c
 * @brief 统一输入检查及虚函数分派。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_base.h"
#include <math.h>
/** @brief 检查控制周期、母线和电压上限的有限性。 */
static bool Valid(const FOC_Input *in)
{
    return isfinite(in->dt_s) && in->dt_s > 0.0f && isfinite(in->bus_voltage_v) && in->bus_voltage_v > 0.0f &&
            isfinite(in->voltage_limit_v) && in->voltage_limit_v > 0.0f;
}
/** @brief 绑定静态虚表，不初始化硬件或分配内存。 */
void FOC_Base_Bind(FOC *self, const FOC_Ops *ops)
{
    self->ops = ops;
}
/** @brief 检查公共输入并进入具体模式。 */
Sys_Status FOC_Enter(FOC *self, const FOC_Input *input)
{
    return Valid(input) ? self->ops->enter(self, input) : SYS_ERROR_PARAM;
}
/** @brief 执行一个控制周期并检查模式输出有限性。 */
Sys_Status FOC_Step(FOC *self, const FOC_Input *input, FOC_Output *output)
{
    if (!Valid(input)) {
        return SYS_ERROR_PARAM;
    }
    FOC_Output result;
    Sys_Status status = self->ops->step(self, input, &result);
    if (status != SYS_OK) {
        return status;
    }
    if (!isfinite(result.ud_v) || !isfinite(result.uq_v) || !isfinite(result.electrical_angle_rad)) {
        return SYS_ERROR_PARAM;
    }
    *output = result;
    return SYS_OK;
}
/** @brief 退出模式并清理局部积分或斜坡状态。 */
void FOC_Exit(FOC *self)
{
    self->ops->exit(self);
}
