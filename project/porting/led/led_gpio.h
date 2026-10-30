/**
 *  @file   led_gpio.h
 *  @brief  实现首成员内嵌 LED 基类的 GPIO 子类。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  porting / led
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 按向上转型组织 LED，分离对象绑定与驱动完整硬件初始化。
 */

#ifndef LED_GPIO_H
#define LED_GPIO_H

#include "led_base.h"
#include "led_pin.h"

/** @brief 普通 GPIO LED 子类，仅提供全亮和熄灭。 */
typedef struct {
    LED base;          /*!< 首成员，用 &对象.base 向上转型。 */
    LED_PinConfig pin; /*!< GPIO 和有效电平配置，构造时按值保存。 */
} LED_GPIO;

/**
 *  @brief  构造普通 GPIO LED 子类。
 *  @param  self 静态 GPIO 子类对象。
 *  @param name 静态名称字符串。
 *  @param pin GPIO 与有效电平配置，按值保存。
 *  @return 无返回值。
 *  @note   只构造 base 和子类属性，不执行硬件配置；参数由 BSP 保证，向上转型使用 &self->base。
 */
void LED_GPIO_Bind(LED_GPIO *self, const char *name, const LED_PinConfig *pin);

#endif
