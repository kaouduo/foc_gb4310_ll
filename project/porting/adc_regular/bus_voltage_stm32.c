/**
 *  @file   bus_voltage_stm32.c
 *  @brief  提供母线电压采样引脚的板级 IO 初始化。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / adc_regular
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 按工程规范统一中文注释和代码格式，保持原有逻辑。
 */

#include "bus_voltage_stm32.h"
#include "bsp_board_config.h"
#include "stm32g4xx_ll_bus.h"

/**
 *  @brief  将母线电压采样引脚配置为无上下拉模拟输入。
 *  @param  无。
 *  @return 无返回值。
 *  @note   启动阶段且 ADC 采样未开始时调用；使能 GPIO 时钟并修改模拟引脚模式，不启动 ADC；无失败状态返回。
 */
void BusVoltage_BoardIO_Init(void)
{
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOB);
    LL_GPIO_SetPinPull(BSP_BOARD_BUS_VOLTAGE_PORT, BSP_BOARD_BUS_VOLTAGE_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(BSP_BOARD_BUS_VOLTAGE_PORT, BSP_BOARD_BUS_VOLTAGE_PIN, LL_GPIO_MODE_ANALOG);
}
