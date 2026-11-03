/**
 *  @file   bsp_encoder.c
 *  @brief  保存板级配置并绑定 MT6826S 协议与 SPI 端点。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  BSP / encoder
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 分离 MT6826S 协议、LL 事务和板级绑定，预留按需读取。
 *  2026/10/30 | kaouduo | V1.1 | 初始化读改写方向位，保留其他配置并校验读回。
 */

#include "bsp_encoder.h"
#include "bsp_board_config.h"
#include "mt6826s_spi.h"
#include "stm32g4xx_ll_bus.h"
static MT6826S encoder;
static MT6826S_SPI endpoint = {.busy = ATOMIC_FLAG_INIT};
static const MT6826S_SPI_Config config = {
        .spi = BSP_BOARD_ENCODER_SPI,
        .baudPrescaler = BSP_BOARD_ENCODER_SPI_PRESCALER,
        .spiClock = LL_APB2_GRP1_PERIPH_SPI1,
        .gpioClocks = LL_AHB2_GRP1_PERIPH_GPIOA | LL_AHB2_GRP1_PERIPH_GPIOC,
        .pins = {{BSP_BOARD_ENCODER_SCK_PORT, BSP_BOARD_ENCODER_SCK_PIN, BSP_BOARD_ENCODER_SCK_AF},
                 {BSP_BOARD_ENCODER_MISO_PORT, BSP_BOARD_ENCODER_MISO_PIN, BSP_BOARD_ENCODER_MISO_AF},
                 {BSP_BOARD_ENCODER_MOSI_PORT, BSP_BOARD_ENCODER_MOSI_PIN, BSP_BOARD_ENCODER_MOSI_AF}},
        .csPort = BSP_BOARD_ENCODER_CS_PORT,
        .csPin = BSP_BOARD_ENCODER_CS_PIN};
/**
 *  @brief  初始化板载 MT6826S 并配置顺时针递增方向。
 *  @param  无。
 *  @return SYS_OK 表示方向寄存器配置及读回一致；其他负值为通信或校验错误。
 *  @note   平台时钟和 DWT 就绪后由启动任务调用一次；读改写方向位并校验，不读角度、不烧写
 * EEPROM；成功不代表磁场和角度反馈正常。
 */
Sys_Status BSP_Encoder_Init(void)
{
    const MT6826S_IO io = {.context = &endpoint, .transfer = MT6826S_SPI_Transfer};
    MT6826S_SPI_Init(&endpoint, &config);
    MT6826S_Bind(&encoder, &io);
    /* 沿用原 HAL 工程 REV6 的 CW 配置，方向定义按芯片顶面观察。 */
    return MT6826S_Init(&encoder, MT6826S_DIRECTION_CW);
}
/**
 *  @brief  按需读取板载编码器的校验有效样本。
 *  @param  sample 有效样本缓冲区。
 *  @return SYS_OK 表示本次样本有效；其他负值表示忙、通信或协议错误。
 *  @note   初始化完成后供未来控制中断或 shell 显式调用；失败保留原输出，当前不安排自动采样。
 */
Sys_Status BSP_Encoder_Read(MT6826S_Sample *sample)
{
    return MT6826S_Read(&encoder, sample);
}
/**
 *  @brief  按需读取编码器诊断寄存器。
 *  @param  address 有效的 12 位寄存器地址。
 *  @param  value 有效的字节输出缓冲区。
 *  @return 正常返回 SYS_OK，其他负值为完整事务错误。
 *  @note   供未来 shell 诊断使用；无 CRC，失败不改变输出，控制运行期间的访问归属由业务协调。
 */
Sys_Status BSP_Encoder_ReadRegister(uint16_t address, uint8_t *value)
{
    return MT6826S_ReadRegister(&encoder, address, value);
}
