/**
 * @file ramp.h
 * @brief 标量及二维矢量斜坡。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef CONTROL_RAMP_H
#define CONTROL_RAMP_H

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 使标量向目标移动，单步变化不超过上限。
 * @param current 当前值。
 * @param target 目标值。
 * @param max_delta 有限非负步长，与输入同单位。
 * @return 本步新值，步长为零时保持原值。
 * @note 调用方提前将速率乘控制周期。
 */
float Ramp_Step(float current, float target, float max_delta);
/**
 * @brief 按二维矢量距离执行一次斜坡。
 * @param x 当前第一分量，原地修改。
 * @param y 当前第二分量，原地修改。
 * @param target_x 目标第一分量。
 * @param target_y 目标第二分量。
 * @param max_delta 有限非负矢量步长，与输入同单位。
 * @return 无。
 * @note 调用方保证有限输入，不对两轴分别硬裁步长。
 */
void Ramp_Vector(float *x, float *y, float target_x, float target_y, float max_delta);

#ifdef __cplusplus
}
#endif
#endif
