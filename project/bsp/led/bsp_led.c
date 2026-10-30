/**
 *  @file   bsp_led.c
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

#include "bsp_led.h"
#include "bsp_board_config.h"
#include "bsp_platform.h"
#include "led_gpio.h"
#include "led_pin.h"
#include "led_pwm.h"

static LED_PWM redLed;
static LED_GPIO greenLed;
LED *BSP_LED_Red;
LED *BSP_LED_Green;

static const LED_PinConfig redPin = {
        .port = BSP_BOARD_LED_R_PORT,
        .pin = BSP_BOARD_LED_R_PIN,
        .gpioClock = BSP_BOARD_LED_GPIO_CLOCK,
        .onLevel = BSP_BOARD_LED_ACTIVE_LOW == 0U,
};
static const LED_PinConfig greenPin = {
        .port = BSP_BOARD_LED_G_PORT,
        .pin = BSP_BOARD_LED_G_PIN,
        .gpioClock = BSP_BOARD_LED_GPIO_CLOCK,
        .onLevel = BSP_BOARD_LED_ACTIVE_LOW == 0U,
};

/**
 *  @brief  构造板载 LED 子类并向上转型为基类句柄。
 *  @param  无。
 *  @return 无返回值。
 *  @note   系统时钟准备完成后在 Board_IO_Init 调用一次；子类静态存储且配置按值复制，不操作外设。
 */
void BSP_LED_Bind(void)
{
    const LED_PWM_Config redConfig = {
            .pin = redPin,
            .timer = BSP_BOARD_LED_R_TIMER,
            .timerClock = BSP_BOARD_LED_TIMER_CLOCK,
            .timerHz = BSP_Platform_ClockHz(),
            .alternate = BSP_BOARD_LED_R_AF,
            .channel = BSP_BOARD_LED_R_CHANNEL,
            .compare = &BSP_BOARD_LED_R_TIMER->CCR1,
    };
    LED_PWM_Bind(&redLed, "red", &redConfig);
    LED_GPIO_Bind(&greenLed, "green", &greenPin);
    /* 显式取得首成员地址，应用只得到基类句柄，不接触硬件子类。 */
    BSP_LED_Red = &redLed.base;
    BSP_LED_Green = &greenLed.base;
}
