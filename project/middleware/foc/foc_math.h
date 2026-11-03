/**
 * @file foc_math.h
 * @brief 弧度归一化、矢量限幅与动态母线限幅。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_MATH_H
#define FOC_MATH_H

#include "system_status.h"
#define FOC_PI 3.14159265358979323846f
#define FOC_TWO_PI (2.0f * FOC_PI)
#define FOC_INV_SQRT3 0.5773502691896258f

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 将弧度归一化到 [0, 2π)。
 * @param angle_rad 输入角度，单位 rad。
 * @return 归一化角度；NaN/Inf 输入产生非有限结果。
 * @note 不改变外部状态。
 */
float FOC_WrapAngle(float angle_rad);
/**
 * @brief 用四象限约简多项式计算单精度正余弦。
 * @param angle_rad 输入角度，单位 rad。
 * @param sine 正弦输出。
 * @param cosine 余弦输出。
 * @return 无；非法角度使两输出均为 NaN。
 * @note 主机验证有效角度的绝对误差小于 2e-6。
 */
void FOC_SinCos(float angle_rad, float *sine, float *cosine);
/**
 * @brief 对二维矢量等比例限幅。
 * @param x 第一分量，原地修改。
 * @param y 第二分量，原地修改。
 * @param limit 非负幅值上限，与分量同单位。
 * @return SYS_OK；非法数值返回 PARAM 且不修改分量。
 * @note 允许上限为零，不对两轴分别硬裁。
 */
Sys_Status FOC_LimitVector(float *x, float *y, float limit);
/**
 * @brief 按母线和两侧占空比裕量计算圆形电压上限。
 * @param bus_v 正母线电压，单位 V。
 * @param duty_min 占空比下界，范围 [0, 0.5)。
 * @param duty_max 占空比上界，范围 (0.5, 1]。
 * @return 可用 dq 幅值，单位 V；非法输入返回 0。
 * @note 以 50% 为中心，不叠加固定电压上限。
 */
float FOC_VoltageLimit(float bus_v, float duty_min, float duty_max);

#ifdef __cplusplus
}
#endif
#endif
