/**
 * @file pi.h
 * @brief 位置式 PI 与外部限幅回算，时间单位为秒。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef CONTROL_PI_H
#define CONTROL_PI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float kp;           /**< 比例增益；电流环使用 V/A。 */
    float ki;           /**< 积分增益；电流环使用 V/(A·s)。 */
    float kaw;          /**< 外部限幅回算增益，单位 1/s。 */
    float integral;     /**< 积分输出，与控制器输出同单位。 */
    float integral_min; /**< 积分输出下限，必须不大于 0。 */
    float integral_max; /**< 积分输出上限，必须不小于 0。 */
} PI_Controller;
/**
 * @brief 检查非负增益和包含零点的积分边界。
 * @param self 待检查配置。
 * @return true 表示数值及范围合法，不改变状态。
 * @note 不判断增益是否适合具体电机。
 */
bool PI_Valid(const PI_Controller *self);
/**
 * @brief 设置积分初值并限制在积分边界内。
 * @param self 已合法配置的 PI。
 * @param integral 有限初值，与输出同单位。
 * @return 无。
 * @note 进入、退出或停机重置时由状态拥有者调用。
 */
void PI_Reset(PI_Controller *self, float integral);
/**
 * @brief 计算候选输出并更新积分。
 * @param self 已合法配置的 PI。
 * @param error 本周期误差。
 * @param dt_s 正控制周期，单位 s。
 * @return 候选输出；计算异常返回 NaN。
 * @note 调用方最终限幅后，须对同一周期调用一次 PI_Track。
 */
float PI_Step(PI_Controller *self, float error, float dt_s);
/**
 * @brief 按实际输出与候选输出之差回算积分。
 * @param self 本周期执行过 PI_Step 的 PI。
 * @param requested 候选输出。
 * @param applied 实际施加输出。
 * @param dt_s 控制周期，单位 s。
 * @return 无；异常时积分为 NaN，由执行器处理。
 * @note 每周期只调用一次，无饱和时两个输出参数相同。
 */
void PI_Track(PI_Controller *self, float requested, float applied, float dt_s);

#ifdef __cplusplus
}
#endif
#endif
