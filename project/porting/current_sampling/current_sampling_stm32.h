/**
 *  @file   current_sampling_stm32.h
 *  @brief  提供双路电流采样引脚的板级 IO 初始化。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / current_sampling
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 按工程规范统一中文注释和代码格式，保持原有逻辑。
 */

#ifndef CURRENT_SAMPLING_STM32_H
#define CURRENT_SAMPLING_STM32_H

/**
 *  @brief  将双路电流采样引脚配置为无上下拉模拟输入。
 *  @param  无。
 *  @return 无返回值。
 *  @note   启动阶段且 ADC 采样未开始时调用；使能 GPIO 时钟并修改模拟引脚模式，不启动 ADC；无失败状态返回。
 */
void CurrentSampling_BoardIO_Init(void);

#endif
