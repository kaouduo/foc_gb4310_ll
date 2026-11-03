/**
 * @file uart_stm32.c
 * @brief USART3 DMA 发送，完成/错误 IRQ 释放缓冲，不访问 application。
 * @author kaouduo
 * @date 2026/11/02
 */
#include "uart_stm32.h"
#include <string.h>
#include "dwt.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_usart.h"
static UART_STM32 *owner;
/** @brief 关闭本通道并锁存错误，任务调用时已屏蔽 DMA IRQ。 */
static void Fail(UART_STM32 *self)
{
    LL_USART_DisableDMAReq_TX(USART3);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ClearFlag_GI1(DMA1);
    ++self->errors;
    self->busy = false;
}
Sys_Status UART_STM32_Init(void *context)
{
    UART_STM32 *self = context;
    const UART_STM32Config *cfg = self->config;
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    LL_AHB2_GRP1_EnableClock(cfg->gpio_clocks);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1 | LL_AHB1_GRP1_PERIPH_DMAMUX1);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USART3);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USART3);
    LL_GPIO_SetPinOutputType(cfg->tx_port, cfg->tx_pin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(cfg->tx_port, cfg->tx_pin, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(cfg->tx_port, cfg->tx_pin, LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_8_15(cfg->tx_port, cfg->tx_pin, cfg->tx_af);
    LL_GPIO_SetPinMode(cfg->tx_port, cfg->tx_pin, LL_GPIO_MODE_ALTERNATE);
    LL_RCC_SetUSARTClockSource(LL_RCC_USART3_CLKSOURCE_PCLK1);
    LL_USART_SetTransferDirection(USART3, LL_USART_DIRECTION_TX);
    LL_USART_ConfigCharacter(USART3, LL_USART_DATAWIDTH_8B, LL_USART_PARITY_NONE, LL_USART_STOPBITS_1);
    LL_USART_SetBaudRate(USART3, cfg->clock_hz, LL_USART_PRESCALER_DIV1, LL_USART_OVERSAMPLING_16, cfg->baud);
    LL_USART_Enable(USART3);
    uint32_t start = DWT_Cycles();
    while (!LL_USART_IsActiveFlag_TEACK(USART3)) {
        if (DWT_ElapsedUs(start) >= 1000U) {
            LL_USART_Disable(USART3);
            return SYS_ERROR_TIMEOUT;
        }
    }
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_1,
                          LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_NORMAL | LL_DMA_PERIPH_NOINCREMENT |
                                  LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE | LL_DMA_MDATAALIGN_BYTE |
                                  LL_DMA_PRIORITY_LOW);
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_USART3_TX);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)&USART3->TDR);
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_1, (uint32_t)self->tx);
    LL_DMA_ClearFlag_GI1(DMA1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    self->completed = self->errors = 0U;
    self->busy = false;
    self->initialized = true;
    owner = self;
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    NVIC_SetPriority(DMA1_Channel1_IRQn, cfg->irq_priority);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    return SYS_OK;
}
Sys_Status UART_STM32_Write(void *context, const uint8_t *data, uint16_t length)
{
    UART_STM32 *self = context;
    if (length == 0U || length > sizeof(self->tx)) {
        return SYS_ERROR_PARAM;
    }
    if (!self->initialized) {
        return SYS_ERROR_NOT_READY;
    }
    /* 单任务拥有提交权；屏蔽此 DMA IRQ，使状态检查和缓冲交接不可被完成/错误 IRQ 打断。
     * 控制采样 IRQ 保持开启，不用全局临界区包围 memcpy。 */
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    if (self->errors) {
        NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        return SYS_ERROR_IO;
    }
    if (self->busy) {
        if (DWT_ElapsedUs(self->started_cycles) < 20000U) {
            NVIC_EnableIRQ(DMA1_Channel1_IRQn);
            return SYS_ERROR_BUSY;
        }
        Fail(self);
        NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        return SYS_ERROR_TIMEOUT;
    }
    memcpy(self->tx, data, length);
    LL_DMA_ClearFlag_GI1(DMA1);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, length);
    self->started_cycles = DWT_Cycles();
    self->busy = true;
    __DMB();
    LL_USART_EnableDMAReq_TX(USART3);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    return SYS_OK;
}
void UART_STM32_Snapshot(void *context, UART_TxStats *out)
{
    UART_STM32 *self = context;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    *out = (UART_TxStats){self->completed, self->errors, self->busy};
    __set_PRIMASK(mask);
}
/** @brief 唯一 DMA1 Channel1 向量，不调用 RTOS/日志。 */
void DMA1_Channel1_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TE1(DMA1)) {
        Fail(owner);
    } else if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
        LL_DMA_ClearFlag_GI1(DMA1);
        ++owner->completed;
        __DMB();
        owner->busy = false;
    }
}
