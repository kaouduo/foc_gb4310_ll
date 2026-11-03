/**
 * @file pi.c
 * @brief 无硬件依赖的 PI 算法。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "pi.h"
#include <math.h>
/** @brief 将积分输出限制在配置边界内。 */
static float Clamp(const PI_Controller *self, float value)
{
    return value < self->integral_min ? self->integral_min : (value > self->integral_max ? self->integral_max : value);
}
/** @brief 检查非负增益和包含零点的积分边界。 */
bool PI_Valid(const PI_Controller *self)
{
    return isfinite(self->kp) && isfinite(self->ki) && isfinite(self->kaw) && self->kp >= 0.0f && self->ki >= 0.0f &&
            self->kaw >= 0.0f && isfinite(self->integral_min) && isfinite(self->integral_max) &&
            self->integral_min <= 0.0f && self->integral_max >= 0.0f;
}
/** @brief 设置积分初值并限制在积分边界内。 */
void PI_Reset(PI_Controller *self, float integral)
{
    self->integral = Clamp(self, integral);
}
/** @brief 计算候选输出并更新积分。 */
float PI_Step(PI_Controller *self, float error, float dt_s)
{
    float next = self->integral + self->ki * error * dt_s;
    float proportional = self->kp * error;
    if (!isfinite(next) || !isfinite(proportional)) {
        return NAN;
    }
    self->integral = Clamp(self, next);
    return proportional + self->integral;
}
/** @brief 按实际输出与候选输出之差回算积分。 */
void PI_Track(PI_Controller *self, float requested, float applied, float dt_s)
{
    float next = self->integral + self->kaw * (applied - requested) * dt_s;
    self->integral = isfinite(next) ? Clamp(self, next) : NAN;
}
