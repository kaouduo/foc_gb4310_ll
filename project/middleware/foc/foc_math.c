/**
 * @file foc_math.c
 * @brief 独立 FOC 数学工具。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_math.h"
#include <math.h>
/** @brief 将弧度归一化到 [0, 2π)。 */
float FOC_WrapAngle(float angle)
{
    if (angle >= 0.0f && angle < FOC_TWO_PI) {
        return angle;
    }
    float value = fmodf(angle, FOC_TWO_PI);
    if (value < 0.0f) {
        value += FOC_TWO_PI;
    }
    return value >= FOC_TWO_PI ? 0.0f : value;
}
/** @brief 对二维矢量等比例限幅。 */
Sys_Status FOC_LimitVector(float *x, float *y, float limit)
{
    if (!isfinite(*x) || !isfinite(*y) || !isfinite(limit) || limit < 0.0f) {
        return SYS_ERROR_PARAM;
    }
    float length = hypotf(*x, *y);
    if (!isfinite(length)) {
        return SYS_ERROR_PARAM;
    }
    if (length > limit) {
        float scale = limit / length;
        *x *= scale;
        *y *= scale;
    }
    return SYS_OK;
}
/** @brief 按母线和两侧占空比裕量计算圆形电压上限。 */
float FOC_VoltageLimit(float bus, float dmin, float dmax)
{
    if (!isfinite(bus) || bus <= 0.0f || !isfinite(dmin) || !isfinite(dmax) || dmin < 0.0f || dmin >= 0.5f ||
        dmax <= 0.5f || dmax > 1.0f) {
        return 0.0f;
    }
    float lower = 0.5f - dmin, upper = dmax - 0.5f;
    return 2.0f * (lower < upper ? lower : upper) * bus * FOC_INV_SQRT3;
}

/** @brief 用四象限约简多项式计算单精度正余弦。 */
void FOC_SinCos(float angle, float *sine, float *cosine)
{
    float wrapped = FOC_WrapAngle(angle);
    if (!isfinite(wrapped)) {
        *sine = *cosine = NAN;
        return;
    }
    int quadrant = (int)(wrapped * (2.0f / FOC_PI) + 0.5f);
    float x = wrapped - quadrant * (FOC_PI * 0.5f);
    float x2 = x * x;
    float s = x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f - x2 / 5040.0f)));
    float c = 1.0f + x2 * (-0.5f + x2 * (1.0f / 24.0f + x2 * (-1.0f / 720.0f + x2 / 40320.0f)));
    switch (quadrant & 3) {
        case 0:
            *sine = s;
            *cosine = c;
            break;
        case 1:
            *sine = c;
            *cosine = -s;
            break;
        case 2:
            *sine = -s;
            *cosine = -c;
            break;
        default:
            *sine = -c;
            *cosine = s;
            break;
    }
}
