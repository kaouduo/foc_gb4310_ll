/**
 * @file foc_current.h
 * @brief 编码器 dq 电流闭环，两个独立 PI。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_CURRENT_H
#define FOC_CURRENT_H

#include "foc_base.h"
#include "pi.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float id_a;         /**< d 轴目标电流，单位 A。 */
    float iq_a;         /**< q 轴目标电流，单位 A。 */
    float ramp_a_per_s; /**< 电流矢量斜坡速率，单位 A/s，必须为正。 */
} FOC_CurrentCommand;
typedef struct {
    FOC base;           /**< 首成员基类。 */
    PI_Controller d_pi; /**< d 轴 PI 状态。 */
    PI_Controller q_pi; /**< q 轴 PI 状态。 */
    FOC_CurrentCommand command;
    float current_limit_a; /**< dq 电流指令矢量上限，单位 A；不是硬件过流门槛。 */
    float applied_d_a;     /**< 斜坡后的 d 轴电流指令，单位 A。 */
    float applied_q_a;     /**< 斜坡后的 q 轴电流指令，单位 A。 */
    bool configured;       /**< 增益和电流指令上限是否已合法配置。 */
    bool entered;          /**< 是否已成功进入模式。 */
} FOC_Current;
/**
 * @brief 构造电流模式，不操作硬件。
 * @param self 调用方持有的对象。
 * @return 无。
 * @note 对象唯一拥有者构造，进入模式不等于允许功率输出。
 */
void FOC_Current_Bind(FOC_Current *self);
/**
 * @brief 停机设置两轴 PI 和电流指令幅值上限。
 * @param self 未进入的模式。
 * @param d d 轴 PI 配置，按值复制。
 * @param q q 轴 PI 配置，按值复制。
 * @param current_limit_a 正指令幅值上限，单位 A。
 * @return SYS_OK；运行中返回 BUSY，非法配置返回 PARAM。
 * @note 清空积分；指令上限不代替过流保护。
 */
Sys_Status FOC_Current_Configure(FOC_Current *self, const PI_Controller *d, const PI_Controller *q,
                                 float current_limit_a);
/**
 * @brief 更新电流命令。
 * @param self 模式对象。
 * @param command 有限目标：dq 电流 A、正斜坡速率 A/s。
 * @return SYS_OK 或 PARAM；非法命令不覆盖原命令。
 * @note 仅对象唯一拥有者在周期边界调用，不与 step 并发。
 */
Sys_Status FOC_Current_SetCommand(FOC_Current *self, const FOC_CurrentCommand *command);

#ifdef __cplusplus
}
#endif
#endif
