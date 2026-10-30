/**
 *  @file   led_pwm.h
 *  @brief  实现首成员内嵌 LED 基类的 PWM 子类。
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

#ifndef LED_PWM_H
#define LED_PWM_H

#include "led_base.h"
#include "led_pin.h"

/** @brief 位于 APB1 的定时器 LED PWM 配置。 */
typedef struct {
    LED_PinConfig pin;          /*!< GPIO 配置，BoardIO 阶段仍保持普通输出。 */
    TIM_TypeDef *timer;         /*!< LED 独占的定时器。 */
    uint32_t timerClock;        /*!< APB1 外设时钟使能掩码。 */
    uint32_t timerHz;           /*!< 实际定时器输入时钟，单位 Hz。 */
    uint32_t alternate;         /*!< 引脚所需 AF 复用编号。 */
    uint32_t channel;           /*!< LL 定时器通道标识。 */
    volatile uint32_t *compare; /*!< 与 channel 对应的 CCR 地址。 */
} LED_PWM_Config;

/** @brief 硬件 PWM LED 子类，支持 0 至 1000 的亮度千分比。 */
typedef struct {
    LED base;              /*!< 首成员，通过 &对象.base 暴露通用接口。 */
    LED_PWM_Config config; /*!< 构造时按值保存，应用层不接触此类型。 */
} LED_PWM;

/**
 *  @brief  构造 PWM LED 子类。
 *  @param  self 静态 PWM 子类对象。
 *  @param name 静态名称字符串。
 *  @param config 有效的定时器、输入频率、通道及对应引脚配置，按值保存。
 *  @return 无返回值。
 *  @note   不使能时钟或写寄存器；定时器与 AF 只在基类 LED_Init 调用本子类 init 时配置。
 */
void LED_PWM_Bind(LED_PWM *self, const char *name, const LED_PWM_Config *config);

#endif
