/**
 *  @file   mt6826s_spi.h
 *  @brief  定义 MT6826S 的 STM32G4 LL 通信端点。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / mt6826s
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 分离 MT6826S 协议、LL 事务和板级绑定，预留按需读取。
 */

#ifndef MT6826S_SPI_H
#define MT6826S_SPI_H
#include <stdatomic.h>
#include "mt6826s_drv.h"
#include "stm32g4xx.h"

/** @brief 单个 SPI 引脚的复用配置。 */
typedef struct {
    GPIO_TypeDef *port; /*!< GPIO 端口。 */
    uint32_t pin;       /*!< 单个 LL_GPIO_PIN_x。 */
    uint32_t alternate; /*!< 对应 LL_GPIO_AF_x。 */
} MT6826S_SPI_Pin;

/** @brief BSP 提供的 SPI1 专用硬件配置。 */
typedef struct {
    SPI_TypeDef *spi;        /*!< 本实现仅使用 SPI1。 */
    uint32_t spiClock;       /*!< SPI1 对应 APB2 时钟及复位掩码。 */
    uint32_t gpioClocks;     /*!< 所有端口的 AHB2 时钟掩码。 */
    MT6826S_SPI_Pin pins[3]; /*!< 顺序为 SCK、MISO、MOSI。 */
    GPIO_TypeDef *csPort;    /*!< 软件片选端口。 */
    uint32_t csPin;          /*!< 低有效片选引脚。 */
} MT6826S_SPI_Config;

/** @brief 专用总线端点，完整事务通过原子标志互斥。 */
typedef struct {
    MT6826S_SPI_Config config; /*!< 初始化时按值保存。 */
    atomic_flag busy;          /*!< 任一上下文持有事务时，其他上下文立即返回 BUSY。 */
} MT6826S_SPI;

/**
 *  @brief  初始化端口、SPI 及通信上下文。
 *  @param  self 静态通信端点。
 *  @param  config 板级有效配置，按值复制。
 *  @return 无返回值。
 *  @note   平台时钟和 DWT 就绪后调用一次；SPI1 专用，模式 3、8 位、32 分频，不发送器件命令。
 */
void MT6826S_SPI_Init(MT6826S_SPI *self, const MT6826S_SPI_Config *config);

/**
 *  @brief  执行单次片选下完整的 SPI 字节事务。
 *  @param  context 已初始化的 MT6826S_SPI 端点。
 *  @param  tx 有效且稳定的 size 字节发送数据。
 *  @param  rx 有效的 size 字节接收空间。
 *  @param  size 非零事务字节数，驱动目前使用 3 或 6。
 *  @return 正常返回 SYS_OK；竞争返回 BUSY；硬件等待超时返回 TIMEOUT，外设错误返回 IO_ERROR。
 *  @note   任务或普通控制中断可调用；零等待抢占互斥，不调用 RTOS、不关中断；失败后 rx 不可用，CS 恢复高并复位 SPI。
 */
Sys_Status MT6826S_SPI_Transfer(void *context, const uint8_t *tx, uint8_t *rx, uint16_t size);
#endif
