/**
 * @file app_motor_internal.h
 * @brief 电机应用内部状态与诊断/运行桥接，不对 BSP 暴露。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef APP_MOTOR_INTERNAL_H
#define APP_MOTOR_INTERNAL_H

#include "app_motor.h"
#include "foc_open_voltage.h"
#include "foc_svpwm.h"
typedef struct {
    FOC_OpenVoltage open;          /**< 当前唯一驻留的运行模式；其他模式接入时再实例化。 */
    const MotorHardware *hardware; /**< BSP 静态绑定，生命周期覆盖整个固件。 */
    bool initialized;
    bool zero_valid;
    volatile bool busy;            /**< 停机采集所有者持有，阻止运行请求。 */
    volatile bool cancel_sampling; /**< 停止入口发布，采集所有者在任务态收尾。 */
    float offset_u;                /**< U 相零偏，ADC count。 */
    float offset_v;                /**< V 相零偏，ADC count。 */
    float bus_v;                   /**< 停机采集时母线，V。 */
    float voltage_limit_v;         /**< 调制范围允许的矢量幅值，V。 */
    float dt_s;                    /**< 采样周期，s。 */
    float current_a_per_count;     /**< 电流换算比例，A/count，符号由板级绑定提供。 */
    App_MotorTest test;
    uint32_t target;
    volatile uint32_t collected; /**< IRQ 写入，停采样后任务读取。 */
    volatile uint32_t errors;
    volatile uint32_t encoder_errors;
    uint32_t sum_u;
    uint32_t sum_v;
    uint16_t min_u;
    uint16_t max_u;
    uint16_t min_v;
    uint16_t max_v;
    FOC_Duty last_duty;          /**< 仅用于关闭输出的干运行诊断，不作为实际 PWM 遥测。 */
    uint32_t encoder_max_cycles; /**< Debug IRQ 编码器最坏耗时，CPU cycles。 */
    uint32_t control_max_cycles; /**< Debug IRQ 算法最坏耗时，CPU cycles。 */
} App_MotorContext;
/** @brief 单轴共享内部对象；采样期间统计由 IRQ 写，监督任务在停采样后读取。 */
extern App_MotorContext motor;

/** @brief 启动时初始化运行状态及加载参数；Flash 和 BSP 已就绪。 */
void App_Motor_RuntimeInit(void);
/** @brief ADC IRQ 分派入口；true 表示运行执行器已消费样本，false 交给诊断。 */
bool App_Motor_RuntimeFastStep(const MotorSample *sample);
/** @brief 是否持有采样或处于排队/准备/运行/收尾阶段；供任务门禁使用。 */
bool App_Motor_RuntimeBusy(void);
/** @brief 准备期采样等待循环读取取消标志，不触碰硬件。 */
bool App_Motor_PreparationCancelled(void);
/** @brief 监督任务在输出关闭时采集 1000 点零偏，约 50 ms；返回采集/质量结果。 */
Sys_Status App_Motor_PrepareZero(void);
/** @brief 输出关闭时独占采集；生产零偏和诊断共用，不允许启用功率输出。 */
Sys_Status App_Motor_Acquire(App_MotorTest test, App_MotorTestResult *out);
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
/** @brief Debug 固定 PWM 通过正常运行执行器排队，复用全部快速保护。 */
Sys_Status App_Motor_RequestPWM(App_MotorTest test);
/** @brief Debug 关闭输出的链路诊断扩展；仅 ADC IRQ 调用。 */
void App_Motor_DiagnosticFastStep(const MotorSample *sample);
/** @brief 任务态进入干运行对象，entered 输出表示是否需要退出。 */
Sys_Status App_Motor_DiagnosticEnter(App_MotorTest test, uint16_t bus_raw, bool *entered);
#endif
#endif
