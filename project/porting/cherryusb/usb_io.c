/**
 *  @file   usb_io.c
 *  @brief  实现 USB 数据线拉低与高阻恢复。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / cherryusb
 *
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 从 BSP 接收引脚配置，不配置 USB 时钟或控制器。
 */
#include "usb_io.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"

void USB_IO_DriveLow(const USB_IO_Config *config)
{
    LL_AHB2_GRP1_EnableClock(config->clock);
    LL_GPIO_ResetOutputPin(config->port, config->pins);
    for (uint32_t pin = 0U; pin < 16U; ++pin) {
        uint32_t mask = 1UL << pin;
        if ((config->pins & mask) != 0U) {
            LL_GPIO_SetPinOutputType(config->port, mask, LL_GPIO_OUTPUT_PUSHPULL);
            LL_GPIO_SetPinSpeed(config->port, mask, LL_GPIO_SPEED_FREQ_HIGH);
            LL_GPIO_SetPinPull(config->port, mask, LL_GPIO_PULL_DOWN);
            LL_GPIO_SetPinMode(config->port, mask, LL_GPIO_MODE_OUTPUT);
        }
    }
}

void USB_IO_Release(const USB_IO_Config *config)
{
    for (uint32_t pin = 0U; pin < 16U; ++pin) {
        uint32_t mask = 1UL << pin;
        if ((config->pins & mask) != 0U) {
            LL_GPIO_SetPinPull(config->port, mask, LL_GPIO_PULL_NO);
            LL_GPIO_SetPinMode(config->port, mask, LL_GPIO_MODE_INPUT);
        }
    }
}
