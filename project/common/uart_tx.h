/**
 * @file uart_tx.h
 * @brief 单任务 UART 异步发送能力，无芯片或 RTOS 依赖。
 * @author kaouduo
 * @date 2026/11/02
 */
#ifndef UART_TX_H
#define UART_TX_H
#include <stdbool.h>
#include <stdint.h>
#include "system_status.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t completed; /**< DMA 已读完的帧数；不表示主机已收到。 */
    uint32_t errors;    /**< 错误/超时锁存计数。 */
    bool busy;          /**< DMA 拥有内部缓冲。 */
} UART_TxStats;
typedef struct {
    void *context;                     /**< BSP 静态实例，覆盖整个固件生命周期。 */
    Sys_Status (*init)(void *context); /**< 启动任务调用一次。 */
    /** @brief 单任务非阻塞提交，复制 length 字节后源缓冲可立即复用。
     * @return OK 接受、BUSY 未接受、PARAM 长度错误、NOT_READY 未初始化、IO 错误锁存。
     */
    Sys_Status (*write)(void *context, const uint8_t *data, uint16_t length);
    void (*snapshot)(void *context, UART_TxStats *out); /**< 任务读取一致副本。 */
} UART_Tx;
#ifdef __cplusplus
}
#endif
#endif
