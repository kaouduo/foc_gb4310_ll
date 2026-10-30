/**
 *  @file   uart_stm32.c
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

#include "uart_stm32.h"
#include "bsp_board_config.h"
#include "stm32g4xx_ll_bus.h"

/**
 *  @brief  配置调试串口发送和接收引脚的 GPIO 复用。
 *  @param  无。
 *  @return 无返回值。
 *  @note   启动阶段且串口通信未开始时调用；使能 GPIO 时钟并修改引脚复用，不启动 USART；无失败状态返回。
 */
void UART_BoardIO_Init(void)
{
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);
    LL_GPIO_SetPinOutputType(BSP_BOARD_DEBUG_TX_PORT, BSP_BOARD_DEBUG_TX_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(BSP_BOARD_DEBUG_TX_PORT, BSP_BOARD_DEBUG_TX_PIN, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(BSP_BOARD_DEBUG_TX_PORT, BSP_BOARD_DEBUG_TX_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_8_15(BSP_BOARD_DEBUG_TX_PORT, BSP_BOARD_DEBUG_TX_PIN, BSP_BOARD_DEBUG_TX_AF);
    LL_GPIO_SetPinMode(BSP_BOARD_DEBUG_TX_PORT, BSP_BOARD_DEBUG_TX_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(BSP_BOARD_DEBUG_RX_PORT, BSP_BOARD_DEBUG_RX_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(BSP_BOARD_DEBUG_RX_PORT, BSP_BOARD_DEBUG_RX_PIN, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(BSP_BOARD_DEBUG_RX_PORT, BSP_BOARD_DEBUG_RX_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_8_15(BSP_BOARD_DEBUG_RX_PORT, BSP_BOARD_DEBUG_RX_PIN, BSP_BOARD_DEBUG_RX_AF);
    LL_GPIO_SetPinMode(BSP_BOARD_DEBUG_RX_PORT, BSP_BOARD_DEBUG_RX_PIN, LL_GPIO_MODE_ALTERNATE);
}
