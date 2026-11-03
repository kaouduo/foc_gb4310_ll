/**
 * @file uart_stm32.h
 * @brief USART3 / DMA1 Channel1 专用异步发送，无 RTOS 依赖。
 * @author kaouduo
 * @date 2026/11/02
 */
#ifndef UART_STM32_H
#define UART_STM32_H
#include "stm32g4xx_ll_gpio.h"
#include "uart_tx.h"
#define UART_STM32_TX_BYTES 68U /**< DMA 缓冲容量，字节。 */
typedef struct {
    GPIO_TypeDef *tx_port;
    uint32_t tx_pin;
    uint32_t tx_af;
    uint32_t gpio_clocks;
    uint32_t clock_hz;     /**< USART3 PCLK1，Hz。 */
    uint32_t baud;         /**< 8N1 波特率。 */
    uint32_t irq_priority; /**< DMA NVIC 抢占优先级；不调用 RTOS。 */
} UART_STM32Config;
typedef struct {
    const UART_STM32Config *config;
    _Alignas(4) uint8_t tx[UART_STM32_TX_BYTES];
    volatile uint32_t completed;
    volatile uint32_t errors;
    volatile bool busy;
    bool initialized;
    uint32_t started_cycles; /**< DWT 起点，用无符号差值处理回绕。 */
} UART_STM32;
/** @brief 启动任务初始化唯一实例；TEACK 等待最多 1 ms。 */
Sys_Status UART_STM32_Init(void *context);
/** @brief 单任务复制 1..68 B 并启动 DMA；不等待完成。
 * @note BUSY 时不接受数据；错误/20 ms 超时锁存，重启初始化恢复。
 */
Sys_Status UART_STM32_Write(void *context, const uint8_t *data, uint16_t length);
/** @brief 任务短临界区读取计数和缓冲占用。 */
void UART_STM32_Snapshot(void *context, UART_TxStats *out);
#endif
