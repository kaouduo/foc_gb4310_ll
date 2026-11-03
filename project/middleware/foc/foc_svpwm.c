/**
 * @file foc_svpwm.c
 * @brief 三相重建与最大最小零序注入。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_svpwm.h"
#include <math.h>
#include <stddef.h>

#define FOC_SQRT3_OVER_2  0.8660254f
#define FOC_DUTY_EPSILON  1.0e-6f


static inline float FOC_Max3(float a, float b, float c)
{
    float max = a;

    if (b > max) {
        max = b;
    }

    if (c > max) {
        max = c;
    }

    return max;
}


static inline float FOC_Min3(float a, float b, float c)
{
    float min = a;

    if (b < min) {
        min = b;
    }

    if (c < min) {
        min = c;
    }

    return min;
}


static inline float FOC_Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}


/**
 * @brief 将 αβ 电压转换为逻辑三相 SVPWM 占空比。
 *
 * @param alpha α 轴参考电压。
 * @param beta  β 轴参考电压。
 * @param bus   直流母线电压。
 * @param duty  输出三相逻辑占空比，范围 [0.0, 1.0]。
 *
 * @return SYS_OK          转换成功。
 * @return SYS_ERROR_PARAM 参数非法或超出可调制范围。
 */
Sys_Status FOC_SVPWM(float alpha,
                     float beta,
                     float bus,
                     FOC_Duty *duty)
{
    if (duty == NULL ||
        !isfinite(alpha) ||
        !isfinite(beta) ||
        !isfinite(bus) ||
        bus <= 0.0f) {
        return SYS_ERROR_PARAM;
    }

    /*
     * Inverse Clarke:
     *
     * U = α
     * V = -α/2 + √3β/2
     * W = -α/2 - √3β/2
     */
    float half_alpha  = -0.5f * alpha;
    float beta_scaled = FOC_SQRT3_OVER_2 * beta;

    float phase_u = alpha;
    float phase_v = half_alpha + beta_scaled;
    float phase_w = half_alpha - beta_scaled;

    /*
     * Min-Max SVPWM:
     *
     * 注入公共模电压，使三相参考电压在母线范围内居中。
     */
    float phase_max = FOC_Max3(phase_u, phase_v, phase_w);
    float phase_min = FOC_Min3(phase_u, phase_v, phase_w);

    float common_mode = -0.5f * (phase_max + phase_min);

    /*
     * 三相参考电压归一化为占空比。
     *
     * 使用母线电压倒数，避免三次浮点除法。
     */
    float inv_bus = 1.0f / bus;

    FOC_Duty result = {
        .u = 0.5f + (phase_u + common_mode) * inv_bus,
        .v = 0.5f + (phase_v + common_mode) * inv_bus,
        .w = 0.5f + (phase_w + common_mode) * inv_bus,
    };

    /*
     * 超出 [0, 1] 较多表示参考电压已经超出线性调制范围。
     * EPSILON 仅用于容忍浮点计算误差。
     */
    if (!isfinite(result.u) ||
        !isfinite(result.v) ||
        !isfinite(result.w) ||
        result.u < -FOC_DUTY_EPSILON ||
        result.u > 1.0f + FOC_DUTY_EPSILON ||
        result.v < -FOC_DUTY_EPSILON ||
        result.v > 1.0f + FOC_DUTY_EPSILON ||
        result.w < -FOC_DUTY_EPSILON ||
        result.w > 1.0f + FOC_DUTY_EPSILON) {
        return SYS_ERROR_PARAM;
    }

    /* 消除浮点误差产生的极小越界。 */
    result.u = FOC_Clamp01(result.u);
    result.v = FOC_Clamp01(result.v);
    result.w = FOC_Clamp01(result.w);

    *duty = result;

    return SYS_OK;
}
