/**
 * @file bsp_vofa.h
 * @brief VOFA USART3 TX / DMA1 Channel1 静态绑定。
 * @author kaouduo
 * @date 2026/11/02
 */
#ifndef BSP_VOFA_H
#define BSP_VOFA_H
#include "uart_tx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** @brief USART3 PC10 AF7，115200 8N1；唯一遥测任务持有发送能力。 */
extern const UART_Tx BSP_VOFA;
#ifdef __cplusplus
}
#endif
#endif
