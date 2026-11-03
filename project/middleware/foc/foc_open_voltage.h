/**
 * @file foc_open_voltage.h
 * @brief 开环电压模式。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_OPEN_VOLTAGE_H
#define FOC_OPEN_VOLTAGE_H

#include "foc_base.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float frequency_hz; /**< 电频率，单位 Hz；负值反转，0 表示固定磁场。 */
    float voltage_v;    /**< d 轴目标幅值，单位 V，非负；q 轴固定为 0。 */
    float ramp_s;       /**< 从当前幅值到新目标的斜坡时间，单位 s，必须为正。 */
} FOC_OpenVoltageCommand;
typedef struct {
    FOC base;                       /**< 首成员基类，控制执行器通过其调用模式。 */
    FOC_OpenVoltageCommand command; /**< 唯一控制执行者持有的当前命令。 */
    float angle_rad;                /**< 积分电角度，范围 [0, 2π)，单位 rad。 */
    float applied_v;                /**< 经过斜坡和动态限幅的幅值，单位 V。 */
    float ramp_rate_v_per_s;        /**< 当前斜坡速率，单位 V/s。 */
    bool entered;                   /**< 是否已经成功进入模式。 */
} FOC_OpenVoltage;
/**
 * @brief 构造开环电压模式，不操作硬件。
 * @param self 调用方持有的对象。
 * @return 无。
 * @note 对象唯一拥有者构造，进入模式不等于允许功率输出。
 */
void FOC_OpenVoltage_Bind(FOC_OpenVoltage *self);
/**
 * @brief 更新开环电压命令。
 * @param self 模式对象。
 * @param command 有限目标：电频率 Hz、非负幅值 V、正斜坡时间 s。
 * @return SYS_OK 或 PARAM；非法命令不覆盖原命令。
 * @note 仅对象唯一拥有者在周期边界调用，不与 step 并发。
 */
Sys_Status FOC_OpenVoltage_SetCommand(FOC_OpenVoltage *self, const FOC_OpenVoltageCommand *command);

#ifdef __cplusplus
}
#endif
#endif
