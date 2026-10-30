/**
 *  @file   led_gpio.c
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

#include "led_gpio.h"
#include <stddef.h>
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"

_Static_assert(offsetof(LED_GPIO, base) == 0U, "LED base must be first");

/**
 *  @brief  根据有效电平设置 GPIO LED 的亮灭。
 *  @param  self 已初始化的 GPIO 子类对象。
 *  @param  on 0 表示熄灭，非零表示点亮。
 *  @return 无返回值。
 *  @note   端口时钟已开启；仅修改对应输出位，不改变模式或对象配置。
 */
static void GpioWrite(LED_GPIO *self, int on)
{
    if ((on != 0) == self->pin.onLevel) {
        LL_GPIO_SetOutputPin(self->pin.port, self->pin.pin);
    } else {
        LL_GPIO_ResetOutputPin(self->pin.port, self->pin.pin);
    }
}

/**
 *  @brief  初始化 GPIO LED 的端口时钟和普通输出。
 *  @param  base 已绑定的 GPIO 子类首成员地址。
 *  @return 无返回值。
 *  @note   启动任务调用一次；先设置熄灭电平再开启输出，不初始化定时器。
 */
static void GpioInit(LED *base)
{
    LED_GPIO *self = (LED_GPIO *)base;
    LL_AHB2_GRP1_EnableClock(self->pin.gpioClock);
    /* 先写熄灭电平，再启用输出。 */
    GpioWrite(self, 0);
    LL_GPIO_SetPinOutputType(self->pin.port, self->pin.pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(self->pin.port, self->pin.pin, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(self->pin.port, self->pin.pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(self->pin.port, self->pin.pin, LL_GPIO_MODE_OUTPUT);
}
/**
 *  @brief  点亮 GPIO LED。
 *  @param  base 已初始化的 GPIO 子类首成员地址。
 *  @return 无返回值。
 *  @note   由基类操作表分派；按绑定的有效电平更新输出。
 */
static void GpioOn(LED *base) { GpioWrite((LED_GPIO *)base, 1); }
/**
 *  @brief  熄灭 GPIO LED。
 *  @param  base 已初始化的 GPIO 子类首成员地址。
 *  @return 无返回值。
 *  @note   由基类操作表分派；按绑定的有效电平更新输出。
 */
static void GpioOff(LED *base) { GpioWrite((LED_GPIO *)base, 0); }
/**
 *  @brief  将两档亮度转换为 GPIO 亮灭。
 *  @param  base 已初始化的 GPIO 子类首成员地址。
 *  @param  brightness 调用方只传 0 或 1000，分别表示熄灭和全亮。
 *  @return 无返回值。
 *  @note   GPIO 子类不提供连续调光；只改变输出电平。
 */
static void GpioBrightness(LED *base, uint16_t brightness) { GpioWrite((LED_GPIO *)base, brightness != 0U); }

static const LED_Ops gpioOps = {
        .init = GpioInit,
        .on = GpioOn,
        .off = GpioOff,
        .setBrightness = GpioBrightness,
};

/**
 *  @brief  构造普通 GPIO LED 子类。
 *  @param  self 静态 GPIO 子类对象。
 *  @param name 静态名称字符串。
 *  @param pin GPIO 与有效电平配置，按值保存。
 *  @return 无返回值。
 *  @note   只构造 base 和子类属性，不执行硬件配置；参数由 BSP 保证，向上转型使用 &self->base。
 */
void LED_GPIO_Bind(LED_GPIO *self, const char *name, const LED_PinConfig *pin)
{
    LED_Base_Bind(&self->base, &gpioOps, name);
    self->pin = *pin;
}
