/**
 * @file foc_base.h
 * @brief FOC 模式基类；无硬件与 RTOS 依赖。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_BASE_H
#define FOC_BASE_H

#include <stdbool.h>
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct FOC FOC;

typedef struct {
    float dt_s;                 /**< 控制周期，单位 s，必须为正。 */
    float bus_voltage_v;        /**< 最近有效母线电压，单位 V。 */
    float voltage_limit_v;      /**< 本周期 dq 矢量幅值上限，单位 V。 */
    float electrical_angle_rad; /**< 标定后的电角度，单位 rad。 */
    float id_a;                 /**< d 轴反馈电流，单位 A。 */
    float iq_a;                 /**< q 轴反馈电流，单位 A。 */
    bool encoder_valid;         /**< 编码器样本是否有效。 */
    bool current_valid;         /**< 本周期电流样本及零偏是否有效。 */
    bool calibration_valid;     /**< 极对数、方向和电角度零偏是否有效。 */
} FOC_Input;

typedef struct {
    float electrical_angle_rad; /**< 本周期调制使用的电角度，单位 rad。 */
    float ud_v;                 /**< d 轴电压，单位 V。 */
    float uq_v;                 /**< q 轴电压，单位 V。 */
} FOC_Output;

typedef struct {
    Sys_Status (*enter)(FOC *self, const FOC_Input *input);

    Sys_Status (*step)(FOC *self, const FOC_Input *input, FOC_Output *output);

    void (*exit)(FOC *self);
} FOC_Ops;

/** @brief 所有子类的首成员；虚表绑定后不修改，单个控制执行者独占状态。 */
struct FOC {
    const FOC_Ops *ops; /**< 绑定到静态常量虚表，运行期间不替换。 */
};

/**
 * @brief 绑定静态虚表，不初始化硬件或分配内存。
 * @param self 待构造基类。
 * @param ops 生命周期覆盖对象的静态常量虚表。
 * @return 无。
 * @note 仅构造阶段调用，后续由一个控制执行者独占状态。
 */
void FOC_Base_Bind(FOC *self, const FOC_Ops *ops);

/**
 * @brief 检查公共输入并进入具体模式。
 * @param self 已绑定基类。
 * @param input 本周期输入，物理量使用 s、V、A、rad。
 * @return SYS_OK、PARAM 或具体模式错误。
 * @note 停机切换模式，调用方负责功率输出关断。
 */
Sys_Status FOC_Enter(FOC *self, const FOC_Input *input);

/**
 * @brief 执行一个控制周期并检查模式输出有限性。
 * @param self 已成功进入的模式。
 * @param input 本周期一致输入。
 * @param output 成功时写入调制角度及 dq 电压。
 * @return SYS_OK 或输入/模式错误，失败不修改 output。
 * @note 唯一拥有者调用；不访问硬件、不打印、不等待。
 */
Sys_Status FOC_Step(FOC *self, const FOC_Input *input, FOC_Output *output);

/**
 * @brief 退出模式并清理局部积分或斜坡状态。
 * @param self 已绑定基类。
 * @return 无。
 * @note 先由调用方关断功率输出，本函数不执行硬件关断。
 */
void FOC_Exit(FOC *self);

#ifdef __cplusplus
}
#endif
#endif
