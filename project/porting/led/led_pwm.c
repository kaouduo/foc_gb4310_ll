/**
 *  @file   led_pwm.c
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

#include "led_pwm.h"
#include <stddef.h>
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_tim.h"

_Static_assert(offsetof(LED_PWM, base) == 0U, "LED base must be first");

/**
 *  @brief  初始化 PWM LED 的 GPIO、定时器和引脚复用。
 *  @param  base 已绑定的 PWM 子类首成员地址。
 *  @return 无返回值。
 *  @note   启动任务调用一次；配置占用的定时器为 1 kHz、1000 级 PWM，初始熄灭后切换复用。
 */
static void PwmInit(LED *base)
{
    LED_PWM_Config *io = &((LED_PWM *)base)->config;
    LL_AHB2_GRP1_EnableClock(io->pin.gpioClock);
    /* 配置期间保持普通输出熄灭，PWM 就绪后再切换复用。 */
    if (io->pin.onLevel) {
        LL_GPIO_ResetOutputPin(io->pin.port, io->pin.pin);
    } else {
        LL_GPIO_SetOutputPin(io->pin.port, io->pin.pin);
    }
    LL_GPIO_SetPinOutputType(io->pin.port, io->pin.pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(io->pin.port, io->pin.pin, LL_GPIO_SPEED_FREQ_LOW);
    LL_GPIO_SetPinPull(io->pin.port, io->pin.pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(io->pin.port, io->pin.pin, LL_GPIO_MODE_OUTPUT);
    LL_APB1_GRP1_EnableClock(io->timerClock);
    LL_TIM_DisableCounter(io->timer);
    LL_TIM_CC_DisableChannel(io->timer, io->channel);
    LL_TIM_SetPrescaler(io->timer, io->timerHz / 1000000U - 1U);
    LL_TIM_SetCounterMode(io->timer, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(io->timer, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetAutoReload(io->timer, 999U);
    LL_TIM_EnableARRPreload(io->timer);
    LL_TIM_OC_SetMode(io->timer, io->channel, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_SetPolarity(io->timer, io->channel, io->pin.onLevel ? LL_TIM_OCPOLARITY_HIGH : LL_TIM_OCPOLARITY_LOW);
    LL_TIM_OC_EnablePreload(io->timer, io->channel);
    *io->compare = 0U;
    LL_TIM_GenerateEvent_UPDATE(io->timer);
    LL_TIM_ClearFlag_UPDATE(io->timer);
    LL_TIM_SetCounter(io->timer, 0U);
    LL_TIM_CC_EnableChannel(io->timer, io->channel);
    LL_TIM_EnableCounter(io->timer);
    /* 先形成熄灭输出，再将普通 GPIO 切换到 PWM 复用，避免启动闪烁。 */
    if (io->pin.pin <= LL_GPIO_PIN_7) {
        LL_GPIO_SetAFPin_0_7(io->pin.port, io->pin.pin, io->alternate);
    } else {
        LL_GPIO_SetAFPin_8_15(io->pin.port, io->pin.pin, io->alternate);
    }
    LL_GPIO_SetPinMode(io->pin.port, io->pin.pin, LL_GPIO_MODE_ALTERNATE);
}

/**
 *  @brief  更新 PWM LED 的亮度比较值。
 *  @param  base 已初始化的 PWM 子类首成员地址。
 *  @param  brightness 亮度千分比，范围 0 至 1000。
 *  @return 无返回值。
 *  @note   由基类操作表分派；更新比较预装载值，在定时器更新事件生效。
 */
static void PwmBrightness(LED *base, uint16_t brightness) { *(((LED_PWM *)base)->config.compare) = brightness; }

/**
 *  @brief  将 PWM LED 设置为全亮。
 *  @param  base 已初始化的 PWM 子类首成员地址。
 *  @return 无返回值。
 *  @note   写入 1000 比较值，不改变 PWM 频率和引脚模式。
 */
static void PwmOn(LED *base) { PwmBrightness(base, 1000U); }
/**
 *  @brief  将 PWM LED 设置为熄灭。
 *  @param  base 已初始化的 PWM 子类首成员地址。
 *  @return 无返回值。
 *  @note   写入 0 比较值，保留定时器运行和引脚复用。
 */
static void PwmOff(LED *base) { PwmBrightness(base, 0U); }

static const LED_Ops pwmOps = {
        .init = PwmInit,
        .on = PwmOn,
        .off = PwmOff,
        .setBrightness = PwmBrightness,
};

/**
 *  @brief  构造 PWM LED 子类。
 *  @param  self 静态 PWM 子类对象。
 *  @param name 静态名称字符串。
 *  @param config 有效的定时器、输入频率、通道及对应引脚配置，按值保存。
 *  @return 无返回值。
 *  @note   不使能时钟或写寄存器；定时器与 AF 只在基类 LED_Init 调用本子类 init 时配置。
 */
void LED_PWM_Bind(LED_PWM *self, const char *name, const LED_PWM_Config *config)
{
    LED_Base_Bind(&self->base, &pwmOps, name);
    self->config = *config;
}
