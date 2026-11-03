/**
 * @file ramp.c
 * @brief 无硬件依赖的斜坡算法。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "ramp.h"
#include <math.h>
/** @brief 使标量向目标移动，单步变化不超过上限。 */
float Ramp_Step(float current, float target, float max_delta)
{
    float delta = target - current;
    return current + (delta > max_delta ? max_delta : (delta < -max_delta ? -max_delta : delta));
}
/** @brief 按二维矢量距离执行一次斜坡。 */
void Ramp_Vector(float *x, float *y, float tx, float ty, float max_delta)
{
    float dx = tx - *x, dy = ty - *y;
    float distance = hypotf(dx, dy);
    if (distance <= max_delta) {
        *x = tx;
        *y = ty;
    } else if (distance > 0.0f) {
        *x += dx * (max_delta / distance);
        *y += dy * (max_delta / distance);
    }
}
