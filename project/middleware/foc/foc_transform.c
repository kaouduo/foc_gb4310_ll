/**
 * @file foc_transform.c
 * @brief 坐标变换纯函数。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "foc_transform.h"
#include "foc_math.h"
/** @brief 执行幅值不变 Clarke 变换，W 按 -U-V 重建。 */
void FOC_Clarke(float u, float v, float *alpha, float *beta)
{
    *alpha = u;
    *beta = (u + 2.0f * v) * FOC_INV_SQRT3;
}
/** @brief 将 αβ 分量旋转到 dq 坐标系。 */
void FOC_Park(float alpha, float beta, float sine, float cosine, float *d, float *q)
{
    *d = alpha * cosine + beta * sine;
    *q = -alpha * sine + beta * cosine;
}
/** @brief 将 dq 分量旋转到 αβ 坐标系。 */
void FOC_InversePark(float d, float q, float sine, float cosine, float *alpha, float *beta)
{
    *alpha = d * cosine - q * sine;
    *beta = d * sine + q * cosine;
}
