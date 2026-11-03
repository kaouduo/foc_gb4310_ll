/**
 * @file bsp_vofa.c
 * @brief 绑定引脚、时钟和速率，不执行协议或任务逻辑。
 * @author kaouduo
 * @date 2026/11/02
 */
#include "bsp_vofa.h"
#include "bsp_board_config.h"
#include "uart_stm32.h"
static const UART_STM32Config config = {
        .tx_port = BSP_BOARD_DEBUG_TX_PORT,
        .tx_pin = BSP_BOARD_DEBUG_TX_PIN,
        .tx_af = BSP_BOARD_DEBUG_TX_AF,
        .gpio_clocks = LL_AHB2_GRP1_PERIPH_GPIOC,
        .clock_hz = 170000000U,
        .baud = BSP_BOARD_VOFA_BAUD,
        .irq_priority = BSP_BOARD_VOFA_DMA_PRIORITY,
};
static UART_STM32 uart = {.config = &config};
const UART_Tx BSP_VOFA = {
        .context = &uart, .init = UART_STM32_Init, .write = UART_STM32_Write, .snapshot = UART_STM32_Snapshot};
