/**
 *  @file   mt6826s_spi.c
 *  @brief  实现不依赖 RTOS 的 MT6826S LL SPI 初始化和有界事务。
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

#include "mt6826s_spi.h"
#include "dwt.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_spi.h"

#define TRANSFER_TIMEOUT_US 30U
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "Encoder requires lock-free atomic flags");

/**
 *  @brief  复位并配置专用 SPI 外设。
 *  @param  self 已保存板级配置的端点。
 *  @return 无返回值。
 *  @note   仅初始化或持有事务的错误恢复路径调用；清空 FIFO 和错误标志，模式 3、8 位、MSB 优先、板级配置分频。
 */
static void ConfigureSPI(MT6826S_SPI *self)
{
    SPI_TypeDef *spi = self->config.spi;
    LL_APB2_GRP1_ForceReset(self->config.spiClock);
    LL_APB2_GRP1_ReleaseReset(self->config.spiClock);
    LL_SPI_SetMode(spi, LL_SPI_MODE_MASTER);
    LL_SPI_SetTransferDirection(spi, LL_SPI_FULL_DUPLEX);
    LL_SPI_SetDataWidth(spi, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetClockPolarity(spi, LL_SPI_POLARITY_HIGH);
    LL_SPI_SetClockPhase(spi, LL_SPI_PHASE_2EDGE);
    LL_SPI_SetNSSMode(spi, LL_SPI_NSS_SOFT);
    LL_SPI_SetBaudRatePrescaler(spi, self->config.baudPrescaler);
    LL_SPI_SetTransferBitOrder(spi, LL_SPI_MSB_FIRST);
    LL_SPI_DisableCRC(spi);
    LL_SPI_SetRxFIFOThreshold(spi, LL_SPI_RX_FIFO_TH_QUARTER);
    LL_SPI_Enable(spi);
}

/**
 *  @brief  初始化端口、SPI 及通信上下文。
 *  @param  self 静态通信端点。
 *  @param  config 板级有效配置，按值复制。
 *  @return 无返回值。
 *  @note   平台时钟和 DWT 就绪后调用一次；SPI1 专用，模式 3、8 位、板级配置分频，不发送器件命令。
 */
void MT6826S_SPI_Init(MT6826S_SPI *self, const MT6826S_SPI_Config *config)
{
    self->config = *config;
    atomic_flag_clear_explicit(&self->busy, memory_order_relaxed);
    LL_AHB2_GRP1_EnableClock(config->gpioClocks);
    /* 先让片选无效，避免切换 SPI 引脚时形成伪事务。 */
    LL_GPIO_SetOutputPin(config->csPort, config->csPin);
    LL_GPIO_SetPinOutputType(config->csPort, config->csPin, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(config->csPort, config->csPin, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetPinPull(config->csPort, config->csPin, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(config->csPort, config->csPin, LL_GPIO_MODE_OUTPUT);
    LL_APB2_GRP1_EnableClock(config->spiClock);
    ConfigureSPI(self);
    for (unsigned i = 0; i < 3U; ++i) {
        const MT6826S_SPI_Pin *pin = &config->pins[i];
        LL_GPIO_SetPinOutputType(pin->port, pin->pin, LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(pin->port, pin->pin, LL_GPIO_SPEED_FREQ_HIGH);
        LL_GPIO_SetPinPull(pin->port, pin->pin, LL_GPIO_PULL_NO);
        if (pin->pin <= LL_GPIO_PIN_7) {
            LL_GPIO_SetAFPin_0_7(pin->port, pin->pin, pin->alternate);
        } else {
            LL_GPIO_SetAFPin_8_15(pin->port, pin->pin, pin->alternate);
        }
        LL_GPIO_SetPinMode(pin->port, pin->pin, LL_GPIO_MODE_ALTERNATE);
    }
    /* 芯片上电典型需要 3 ms；留 5 ms 后才允许后续主动读取，不以此判定在线。 */
    (void)DWT_DelayUs(5000U);
}

/**
 *  @brief  在共享事务截止时间前等待 SPI 标志。
 *  @param  spi 专用外设。
 *  @param  mask 待检查的 SR 位。
 *  @param  value 预期掩码值。
 *  @param  start 本次完整事务的 DWT 起始周期。
 *  @return 返回正常、硬件错误或超时结果。
 *  @note   DWT 已启用；轮询不依赖 tick，等待预算覆盖整帧，不是每个字节单独计时。
 */
static Sys_Status WaitFlag(SPI_TypeDef *spi, uint32_t mask, uint32_t value, uint32_t start)
{
    for (;;) {
        uint32_t status = spi->SR;
        if ((status & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE)) != 0U) {
            return SYS_ERROR_IO;
        }
        if ((status & mask) == value) {
            return SYS_OK;
        }
        if (DWT_ElapsedUs(start) >= TRANSFER_TIMEOUT_US) {
            return SYS_ERROR_TIMEOUT;
        }
    }
}

/**
 *  @brief  执行单次片选下完整的 SPI 字节事务。
 *  @param  context 已初始化的 MT6826S_SPI 端点。
 *  @param  tx 有效且稳定的 size 字节发送数据。
 *  @param  rx 有效的 size 字节接收空间。
 *  @param  size 非零事务字节数，驱动目前使用 3 或 6。
 *  @return 正常返回 SYS_OK；竞争返回 BUSY；硬件等待超时返回 TIMEOUT，外设错误返回 IO_ERROR。
 *  @note   任务或普通控制中断可调用；零等待抢占互斥，不调用 RTOS、不关中断；失败后 rx 不可用，CS 恢复高并复位 SPI。
 */
Sys_Status MT6826S_SPI_Transfer(void *context, const uint8_t *tx, uint8_t *rx, uint16_t size)
{
    MT6826S_SPI *self = context;
    if (atomic_flag_test_and_set_explicit(&self->busy, memory_order_acquire)) {
        return SYS_ERROR_BUSY;
    }
    SPI_TypeDef *spi = self->config.spi;
    uint32_t start = DWT_Cycles();
    Sys_Status result = SYS_OK;
    LL_GPIO_ResetOutputPin(self->config.csPort, self->config.csPin);
    (void)DWT_DelayUs(1U);
    /* 同一 CS 内以最多两个未接收字节流水收发，避免逐字节软件间隙；不改变 SPI 频率。 */
    uint16_t sent = 0U, received = 0U;
    uint32_t budget = (SystemCoreClock / 1000000U) * TRANSFER_TIMEOUT_US;
    while (received < size) {
        uint32_t flags = spi->SR;
        if (flags & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE)) {
            result = SYS_ERROR_IO;
            break;
        }
        if (flags & SPI_SR_RXNE) {
            rx[received++] = LL_SPI_ReceiveData8(spi);
        }
        if (sent < size && (uint16_t)(sent - received) < 2U && (flags & SPI_SR_TXE)) {
            LL_SPI_TransmitData8(spi, tx[sent++]);
        }
        if ((uint32_t)(DWT->CYCCNT - start) >= budget) {
            result = SYS_ERROR_TIMEOUT;
            break;
        }
    }
    if (result == SYS_OK) {
        result = WaitFlag(spi, SPI_SR_BSY, 0U, start);
    }
    (void)DWT_DelayUs(1U);
    LL_GPIO_SetOutputPin(self->config.csPort, self->config.csPin);
    if (result != SYS_OK) {
        ConfigureSPI(self);
    }
    (void)DWT_DelayUs(1U);
    atomic_flag_clear_explicit(&self->busy, memory_order_release);
    return result;
}
