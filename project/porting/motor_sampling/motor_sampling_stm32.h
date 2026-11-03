/**
 * @file motor_sampling_stm32.h
 * @brief TIM1/双 ADC 单一资源拥有者，供 BSP bind。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef MOTOR_SAMPLING_STM32_H
#define MOTOR_SAMPLING_STM32_H
#include "motor_io.h"
#include "stm32g4xx.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    GPIO_TypeDef *port; /**< 静态 GPIO 端口。 */
    uint32_t pin;       /**< LL_GPIO_PIN_* 引脚掩码。 */
    uint32_t alternate; /**< LL_GPIO_AF_* 复用值，模拟引脚不使用。 */
} MotorPin;
typedef struct {
    MotorPin pwm[6];         /**< CH1/CH2/CH3/CH1N/CH2N/CH3N 顺序的引脚配置。 */
    MotorPin analog[3];      /**< 主 ADC 电流、从 ADC 电流、母线模拟引脚。 */
    uint32_t master_channel; /**< ADC1 注入电流通道，映射逻辑 V。 */
    uint32_t slave_channel;  /**< ADC2 注入电流通道，映射逻辑 U。 */
    uint32_t bus_channel;    /**< ADC1 规则组母线通道。 */
    uint32_t gpio_clocks;    /**< 所有 PWM 与模拟引脚的 AHB2 GPIO 时钟掩码。 */
    uint32_t timer_hz;       /**< TIM1 输入时钟，单位 Hz。 */
    uint32_t pwm_hz;         /**< 完整 PWM 周期频率，单位 Hz。 */
    uint32_t deadtime_ns;    /**< 互补输出死区，单位 ns。 */
    uint32_t irq_priority;   /**< ADC/TIM IRQ 优先级，不允许调用 RTOS。 */
    uint8_t u_channel;       /**< 逻辑 U 对应 TIM1 通道编号，取 1～3。 */
    uint8_t v_channel;       /**< 逻辑 V 对应 TIM1 通道编号。 */
    uint8_t w_channel;       /**< 逻辑 W 对应 TIM1 通道编号。 */
    float duty_min, duty_max;
} MotorSTM32Config;
typedef struct {
    const MotorSTM32Config *config; /**< BSP 静态配置，运行中只读。 */
    MotorSampleCallback callback;   /**< 仅在采样停止时修改的同步回调。 */
    void *callback_context;         /**< 随回调借用的静态上下文。 */
    volatile bool initialized, sampling;
    volatile uint32_t samples, errors, interval_min, interval_max, irq_max, lease_periods;
    volatile uint32_t adc_errors, deadline_errors, deadline_min;
    uint32_t previous_cycles; /**< 上一次完成 IRQ 的 DWT 周期数。 */
} MotorSTM32;
/**
 * @brief 绑定 TIM1/ADC1/ADC2 专用实例及 IRQ 目标。
 * @param self BSP 静态实例。
 * @param config 覆盖实例生命周期的静态配置，固定使用 TIM1。
 * @return 无。
 * @note 调度前调用一次，不支持同时绑定多个中断拥有者。
 */
void MotorSTM32_Bind(MotorSTM32 *self, const MotorSTM32Config *config);
/**
 * @brief 初始化安全 PWM 引脚、TIM1 及双 ADC。
 * @param context 已绑定的 MotorSTM32 实例。
 * @return SYS_OK 或 ADC 校准/就绪超时、配置错误。
 * @note 调度前调用一次，时钟和 DWT 已就绪；不启动采样和功率输出。
 */
Sys_Status MotorSTM32_Init(void *context);
/**
 * @brief 在采样停止时注册同步 IRQ 回调。
 * @param context 已绑定实例。
 * @param callback 非阻塞快速回调，不调用 RTOS。
 * @param user 覆盖采样过程的静态回调上下文。
 * @return SYS_OK 或 BUSY。
 * @note 任务调用；回调的 sample 只在当次同步调用中有效。
 */
Sys_Status MotorSTM32_SetCallback(void *context, MotorSampleCallback callback, void *user);
/**
 * @brief 清除统计并启动 TIM1 触发双 ADC 采样。
 * @param context 已初始化且注册回调的实例。
 * @return SYS_OK、BUSY 或 NOT_READY。
 * @note 任务调用，不使能 MOE，功率输出另行授权。
 */
Sys_Status MotorSTM32_Start(void *context);
/**
 * @brief 关闭输出、停止定时器和注入采样。
 * @param context 已绑定实例。
 * @return SYS_OK 或 ADC 停止超时。
 * @note 任务调用，可重复关闭；最多轮询等待 1000 μs。
 */
Sys_Status MotorSTM32_Stop(void *context);
/**
 * @brief 用规则组读取一次母线 ADC。
 * @param context 已初始化实例。
 * @param raw 成功时写入 ADC 原始计数。
 * @return SYS_OK、NOT_READY 或超时。
 * @note 单任务拥有，可与注入采样并行；超时恢复可能另等待一次停止。
 */
Sys_Status MotorSTM32_ReadBus(void *context, uint16_t *raw);
/**
 * @brief 保存并屏蔽 IRQ，开始有界状态事务。
 * @return 原 PRIMASK，须传入匹配的 CriticalExit。
 * @note 任务或 IRQ 可用；事务内不得阻塞或调用 RTOS。
 */
uint32_t MotorSTM32_CriticalEnter(void);
/**
 * @brief 结束事务并恢复原 IRQ 掩码。
 * @param mask 对应 CriticalEnter 返回的 PRIMASK。
 * @return 无。
 * @note 按嵌套顺序恢复，不强制开启原本关闭的 IRQ。
 */
void MotorSTM32_CriticalExit(uint32_t mask);
/**
 * @brief 在短临界区复制硬件状态与统计。
 * @param context 已绑定实例。
 * @param snapshot 输出副本。
 * @return 无。
 * @note 任务调用，保存并恢复原 IRQ 掩码，耗时单位为 CPU cycles。
 */
void MotorSTM32_Snapshot(void *context, MotorHardwareSnapshot *snapshot);

#ifdef __cplusplus
}
#endif
#endif
