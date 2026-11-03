/**
 * @file pwm_output_stm32.h
 * @brief 三相输出能力，不重复初始化 TIM1。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef PWM_OUTPUT_STM32_H
#define PWM_OUTPUT_STM32_H
#include "motor_io.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 原子更新三相占空比预装载值。
 * @param context 已初始化的 MotorSTM32 实例。
 * @param u 逻辑 U 相占空比。
 * @param v 逻辑 V 相占空比。
 * @param w 逻辑 W 相占空比。
 * @return SYS_OK；占空比越界或更新截止错误时关断输出。
 * @note 任务或 ADC IRQ 可用，仅修正浮点舍入，不裁剪真实越界请求。
 */
Sys_Status PWM_Output_Write(void *context, float u, float v, float w);
/**
 * @brief 按有限 PWM 周期数使能三相输出。
 * @param context 已开始同步采样且无硬件错误的实例。
 * @param periods 完整周期数，范围 1～90*pwm_hz。
 * @return SYS_OK、PARAM、NOT_READY 或 BUSY。
 * @note 任务调用，不检查业务零偏与电流；调用方必须已建立保护。
 */
Sys_Status PWM_Output_Enable(void *context, uint32_t periods);
/**
 * @brief 按最多一秒的租期使能诊断输出。
 * @param context 已开始同步采样的实例。
 * @param periods 完整周期数，不超过 pwm_hz。
 * @return SYS_OK 或参数/硬件状态错误。
 * @note 任务调用，时长限制不代替电流保护。
 */
Sys_Status PWM_Output_EnableTest(void *context, uint32_t periods);
/**
 * @brief 清除 MOE、相输出 CCER 和剩余周期。
 * @param context 已绑定的静态实例。
 * @return 无。
 * @note 任务或 IRQ 均可调用，不等待，不停止 ADC 采样。
 */
void PWM_Output_Disable(void *context);
/**
 * @brief 直接关断 TIM1 功率输出，不依赖上下文。
 * @return 无。
 * @note 任务、IRQ 或断言路径可用；不更新业务状态，不停止采样。
 */
void PWM_Output_EmergencyOff(void);

#ifdef __cplusplus
}
#endif
#endif
