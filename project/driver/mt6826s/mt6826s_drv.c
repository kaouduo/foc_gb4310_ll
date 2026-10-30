/**
 *  @file   mt6826s_drv.c
 *  @brief  实现 MT6826S 连续读取、CRC 和固定字段校验。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  driver / mt6826s
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 分离 MT6826S 协议、LL 事务和板级绑定，预留按需读取。
 *  2026/10/30 | kaouduo | V1.1 | 初始化读改写方向位，保留其他配置并校验读回。
 */

#include "mt6826s_drv.h"
/**
 *  @brief  计算角度高字节、低字节及状态字节的 CRC-8。
 *  @param  data 按线序排列的三个受保护字节。
 *  @return 多项式 0x07、初始值 0、无反射及末尾异或的 CRC。
 *  @note   纯计算，不依赖平台；不包含两个命令阶段字节。
 */
static uint8_t Crc8(const uint8_t data[3])
{
    uint8_t crc = 0U;
    for (uint16_t i = 0; i < 3U; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8U; ++bit)
            crc = (uint8_t)((crc & 0x80U) ? ((uint32_t)crc << 1U) ^ 0x07U : (uint32_t)crc << 1U);
    }
    return crc;
}
/**
 *  @brief  绑定芯片驱动的完整 SPI 事务接口。
 *  @param  self 生命周期覆盖使用期的静态驱动对象。
 *  @param  io 有效通信接口，配置按值保存。
 *  @return 无返回值。
 *  @note   仅构造对象，不访问硬件；调用方保证指针有效，运行中不重新绑定。
 */
void MT6826S_Bind(MT6826S *self, const MT6826S_IO *io) { self->io = *io; }
/**
 *  @brief  读取并校验一帧 15 位机械角度。
 *  @param  self 已绑定且硬件已初始化的驱动对象。
 *  @param  sample 有效输出缓冲区，角度范围为 0 至 32767。
 *  @return SYS_OK 表示 CRC、固定位和状态均正常；其他负值表示事务或协议错误。
 *  @note   一次 6 字节连续事务；任何失败均保留原输出，不做速度估算或电角度换算。
 */
Sys_Status MT6826S_Read(MT6826S *self, MT6826S_Sample *sample)
{
    const uint8_t tx[6] = {0xA0U, 0x03U, 0U, 0U, 0U, 0U};
    uint8_t rx[6];
    Sys_Status result = self->io.transfer(self->io.context, tx, rx, 6U);
    if (result != SYS_OK)
        return result;
    /* 先确认帧完整性，再解释状态，坏帧绝不能覆盖上次有效角度。 */
    if (Crc8(&rx[2]) != rx[5])
        return SYS_ERROR_CRC;
    if ((rx[3] & 1U) != 0U || (rx[4] & 0xF8U) != 0U)
        return SYS_ERROR_FRAME;
    if ((rx[4] & 7U) != 0U)
        return SYS_ERROR_DEVICE;
    *sample = (MT6826S_Sample){.angleRaw = (uint16_t)(((uint16_t)rx[2] << 7U) | (rx[3] >> 1U)), .status = rx[4]};
    return SYS_OK;
}
/**
 *  @brief  读取单个芯片寄存器。
 *  @param  self 已绑定且硬件已初始化的驱动对象。
 *  @param  address 调用方保证为 0x000 至 0xFFF 的寄存器地址。
 *  @param  value 有效的单字节输出地址。
 *  @return SYS_OK 表示通信完成；其他负值透传事务错误。
 *  @note   仅供后续诊断按需使用；单寄存器读取没有 CRC，失败不更新输出，不用于分步读取角度锁存区。
 */
Sys_Status MT6826S_ReadRegister(MT6826S *self, uint16_t address, uint8_t *value)
{
    const uint8_t tx[3] = {(uint8_t)(0x30U | (address >> 8U)), (uint8_t)address, 0U};
    uint8_t rx[3];
    Sys_Status result = self->io.transfer(self->io.context, tx, rx, 3U);
    if (result == SYS_OK)
        *value = rx[2];
    return result;
}

/**
 *  @brief  配置芯片角度递增方向并验证寄存器。
 *  @param  self 已绑定且硬件完成上电等待的驱动对象。
 *  @param  direction 有效的 CW 或 CCW 方向，按手册芯片顶面观察定义。
 *  @return SYS_OK 表示完整寄存器读回一致；不一致返回 FRAME_ERROR，通信错误透传。
 *  @note   仅启动阶段、采样前调用；读改写 0x00D bit3，保留其他位，不烧写 EEPROM、不读取角度；失败后方向不确定。
 */
Sys_Status MT6826S_Init(MT6826S *self, MT6826S_Direction direction)
{
    uint8_t original;
    Sys_Status result = MT6826S_ReadRegister(self, 0x00DU, &original);
    if (result != SYS_OK)
        return result;
    const uint8_t wanted = (uint8_t)((original & (uint8_t)~0x08U) | ((uint8_t)direction << 3U));
    if (original != wanted) {
        const uint8_t tx[3] = {0x60U, 0x0DU, wanted};
        uint8_t rx[3];
        result = self->io.transfer(self->io.context, tx, rx, 3U);
        if (result != SYS_OK)
            return result;
    }
    uint8_t actual;
    result = MT6826S_ReadRegister(self, 0x00DU, &actual);
    if (result != SYS_OK)
        return result;
    return actual == wanted ? SYS_OK : SYS_ERROR_FRAME;
}
