/**
 *  @file   uart_stm32.h
 *  @brief  提供调试串口引脚的板级 IO 初始化。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / uart
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 按工程规范统一中文注释和代码格式，保持原有逻辑。
 */

#ifndef UART_STM32_H
#define UART_STM32_H

/**
 *  @brief  配置调试串口发送和接收引脚的 GPIO 复用。
 *  @param  无。
 *  @return 无返回值。
 *  @note   启动阶段且串口通信未开始时调用；使能 GPIO 时钟并修改引脚复用，不启动 USART；无失败状态返回。
 */
void UART_BoardIO_Init(void);

#endif
