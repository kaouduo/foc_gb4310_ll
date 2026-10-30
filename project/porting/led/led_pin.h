/**
 *  @file   led_pin.h
 *  @brief  定义 LED 驱动共用的引脚配置。
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

#ifndef LED_PIN_H
#define LED_PIN_H
#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx.h"

/** @brief LED 驱动引脚配置，不含定时器配置。 */
typedef struct {
    GPIO_TypeDef *port; /*!< GPIO 端口。 */
    uint32_t pin;       /*!< 单个 LL_GPIO_PIN_x 掩码。 */
    uint32_t gpioClock; /*!< AHB2 GPIO 时钟使能掩码。 */
    bool onLevel;       /*!< true 为高电平亮，false 为低电平亮。 */
} LED_PinConfig;

#endif
