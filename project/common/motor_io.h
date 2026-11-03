/**
 * @file motor_io.h
 * @brief 电机同步样本与硬件能力定义。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef MOTOR_IO_H
#define MOTOR_IO_H

#include <stdbool.h>
#include <stdint.h>
#include "system_status.h"

#ifdef __cplusplus
extern "C" {

#endif
#define MOTOR_SAMPLE_VALID 1U
#define MOTOR_SAMPLE_ADC_ERROR 2U

typedef struct {
    uint16_t current_u_raw;    /**< U 相同步 ADC 原始计数，未扣零偏。 */
    uint16_t current_v_raw;    /**< V 相同步 ADC 原始计数，未扣零偏。 */
    uint32_t sequence;         /**< 本次采样启动后的递增序号，首个样本为 1。 */
    uint32_t flags;            /**< MOTOR_SAMPLE_* 标志，由接收方检查有效性。 */
    uint32_t completed_cycles; /**< 完成 IRQ 入口的 DWT 周期数，允许无符号回绕。 */
} MotorSample;

/** @brief IRQ 同步调用；不保存 sample 指针，不调用 RTOS/日志/等待锁。 */
typedef void (*MotorSampleCallback)(void *context, const MotorSample *sample);

typedef struct {
    uint32_t samples;             /**< 本次采样回调次数。 */
    uint32_t errors;              /**< ADC 与执行截止错误总数。 */
    uint32_t adc_errors;          /**< 双 ADC 同步、完成或溢出错误数。 */
    uint32_t deadline_errors;     /**< 控制更新截止错误数。 */
    uint32_t deadline_min_cycles; /**< IRQ 入口距更新截止的最小预算，CPU cycles。 */
    uint32_t interval_min_cycles; /**< 相邻完成 IRQ 的最小间隔，CPU cycles。 */
    uint32_t interval_max_cycles; /**< 相邻完成 IRQ 的最大间隔，CPU cycles。 */
    uint32_t irq_max_cycles;      /**< 含同步回调的最长 IRQ 执行时间，CPU cycles。 */
    uint32_t timer_hz;            /**< 定时器计数时钟，单位 Hz。 */
    uint32_t period_counts;       /**< 中心对齐定时器 ARR，不是完整周期计数。 */
    uint32_t compare_u;           /**< U 相 CCR 计数。 */
    uint32_t compare_v;           /**< V 相 CCR 计数。 */
    uint32_t compare_w;           /**< W 相 CCR 计数。 */
    bool initialized;             /**< 外设初始化是否完成。 */
    bool sampling;                /**< 是否已启动同步采样流程。 */
    bool output_enabled;          /**< 硬件 MOE 是否使能。 */
} MotorHardwareSnapshot;

typedef struct {
    float adc_reference_v; /**< ADC 换算参考电压，单位 V。 */
    float counts;          /**< ADC 满量程计数，例如 12 位为 4096。 */
    float current_gain;    /**< 电流放大器增益，无量纲。 */
    float shunt_ohm;       /**< 分流电阻，单位 Ω。 */
    float bus_ratio;       /**< 母线分压还原倍率，无量纲。 */
    float current_u_sign;  /**< U 相电流极性，取 +1 或 -1。 */
    float current_v_sign;  /**< V 相电流极性，取 +1 或 -1。 */
    float duty_min;        /**< 允许的占空比下界，低于 0.5。 */
    float duty_max;        /**< 允许的占空比上界，高于 0.5。 */
    uint32_t pwm_hz;       /**< PWM 完整周期及同步采样频率，单位 Hz。 */
} MotorScale;

/** @brief BSP 静态能力；启停/读取/快照仅任务调用，写占空比/关断/临界事务也供控制 IRQ 使用。 */
typedef struct {
    void *context; /**< BSP 持有的静态 porting 实例，业务不解释其内容。 */
    const MotorScale *scale; /**< BSP 静态换算及调制边界，运行中只读。 */
    Sys_Status (*init)(void *context); /**< 调度前一次初始化，不启动采样或输出。 */
    Sys_Status (*set_sample_callback)(void *context, MotorSampleCallback callback, void *user); /**< 停采样注册。 */
    Sys_Status (*start_sampling)(void *context); /**< 启动同步 ADC，不使能功率输出。 */
    Sys_Status (*stop_sampling)(void *context); /**< 关断输出并有界等待 ADC 停止。 */
    Sys_Status (*read_bus)(void *context, uint16_t *raw); /**< 独占规则组读取原始 ADC 计数。 */
    Sys_Status (*write_duty)(void *context, float u, float v, float w); /**< 写逻辑 U/V/W 占空比预装载。 */
    /** @brief 最多 1 秒，TIM 更新 IRQ 独立计时关断；用于已确认台架的波形检查。 */
    Sys_Status (*enable_test_output)(void *context, uint32_t periods);

    /** @brief 有限周期运行输出，最长 90 s；应用负责零偏、保护和故障门禁。 */
    Sys_Status (*enable_output)(void *context, uint32_t periods);

    /** @brief 有界控制状态事务；保存/恢复原中断掩码，不等待、不调用 RTOS。 */
    uint32_t (*critical_enter)(void);

    void (*critical_exit)(uint32_t mask);

    void (*disable_output)(void *context);                            /**< 幂等立即关断，不等待。 */
    void (*snapshot)(void *context, MotorHardwareSnapshot *snapshot); /**< 复制一致硬件快照。 */
} MotorHardware;

#ifdef __cplusplus
}
#endif
#endif
