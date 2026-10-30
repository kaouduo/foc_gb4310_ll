/**
 *  @file   bsp_led.h
 *  @brief  隐藏具体 LED 子类并公开红绿灯基类句柄。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  bsp / led
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 按向上转型组织 LED，分离对象绑定与驱动完整硬件初始化。
 */

#ifndef BSP_LED_H
#define BSP_LED_H
#include "led_base.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

extern LED *BSP_LED_Red;
extern LED *BSP_LED_Green;

/**
 *  @brief  构造板载 LED 子类并向上转型为基类句柄。
 *  @param  无。
 *  @return 无返回值。
 *  @note   系统时钟准备完成后在 Board_IO_Init 调用一次；子类静态存储且配置按值复制，不操作外设。
 */
void BSP_LED_Bind(void);

#ifdef __cplusplus
}
#endif

#endif
