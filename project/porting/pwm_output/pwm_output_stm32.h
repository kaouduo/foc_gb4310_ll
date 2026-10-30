/**
 *  @file   pwm_output_stm32.h
 *  @brief  提供三相 PWM 输出引脚的板级 IO 初始化。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / pwm_output
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 按工程规范统一中文注释和代码格式，保持原有逻辑。
 */

#ifndef PWM_OUTPUT_STM32_H
#define PWM_OUTPUT_STM32_H

/**
 *  @brief  配置 TIM1 三相主输出和互补输出的 GPIO 复用。
 *  @param  无。
 *  @return 无返回值。
 *  @note   启动阶段且 TIM1 输出关闭时调用；使能 GPIO 时钟并修改六路引脚配置，不启动 PWM；无失败状态返回。
 */
void PWM_Output_BoardIO_Init(void);

#endif
