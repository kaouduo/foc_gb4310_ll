/**
 *  @file   mt6826s_drv.h
 *  @brief  定义平台无关的 MT6826S 读取与方向初始化接口。
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

#ifndef MT6826S_DRV_H
#define MT6826S_DRV_H

#include <stdint.h>
#include "system_status.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

#define MT6826S_COUNTS_PER_TURN 32768U

/** @brief 按数据手册定义的正向角度递增方向。 */
typedef enum {
    MT6826S_DIRECTION_CCW = 0, /*!< 逆时针递增，ROT_DIR 为 0。 */
    MT6826S_DIRECTION_CW = 1   /*!< 顺时针递增，ROT_DIR 为 1。 */
} MT6826S_Direction;
/** @brief 校验通过的芯片原始样本。 */
typedef struct {
    uint16_t angleRaw; /*!< 单圈机械角计数，0 至 32767。 */
    uint8_t status;    /*!< 芯片三位状态，正常样本为 0。 */
} MT6826S_Sample;

/** @brief 通信依赖，由 porting 完成整帧 CS 时序、收发及互斥。 */
typedef struct {
    void *context; /*!< 生命周期覆盖设备使用期的硬件上下文。 */
    Sys_Status (*transfer)(void *context, const uint8_t *tx, uint8_t *rx,
                           uint16_t size); /*!< 单次 CS 下连续 size 字节，失败后 CS 恢复高电平。 */
} MT6826S_IO;

/** @brief 单个 MT6826S 的静态驱动对象，不保存业务状态。 */
typedef struct {
    MT6826S_IO io; /*!< 构造时复制的通信接口。 */
} MT6826S;

/**
 *  @brief  绑定芯片驱动的完整 SPI 事务接口。
 *  @param  self 生命周期覆盖使用期的静态驱动对象。
 *  @param  io 有效通信接口，配置按值保存。
 *  @return 无返回值。
 *  @note   仅构造对象，不访问硬件；调用方保证指针有效，运行中不重新绑定。
 */
void MT6826S_Bind(MT6826S *self, const MT6826S_IO *io);

/**
 *  @brief  读取并校验一帧 15 位机械角度。
 *  @param  self 已绑定且硬件已初始化的驱动对象。
 *  @param  sample 有效输出缓冲区，角度范围为 0 至 32767。
 *  @return SYS_OK 表示 CRC、固定位和状态均正常；其他负值表示事务或协议错误。
 *  @note   一次 6 字节连续事务；任何失败均保留原输出，不做速度估算或电角度换算。
 */
Sys_Status MT6826S_Read(MT6826S *self, MT6826S_Sample *sample);

/**
 *  @brief  读取单个芯片寄存器。
 *  @param  self 已绑定且硬件已初始化的驱动对象。
 *  @param  address 调用方保证为 0x000 至 0xFFF 的寄存器地址。
 *  @param  value 有效的单字节输出地址。
 *  @return SYS_OK 表示通信完成；其他负值透传事务错误。
 *  @note   仅供后续诊断按需使用；单寄存器读取没有 CRC，失败不更新输出，不用于分步读取角度锁存区。
 */
Sys_Status MT6826S_ReadRegister(MT6826S *self, uint16_t address, uint8_t *value);

/**
 *  @brief  配置芯片角度递增方向并验证寄存器。
 *  @param  self 已绑定且硬件完成上电等待的驱动对象。
 *  @param  direction 有效的 CW 或 CCW 方向，按手册芯片顶面观察定义。
 *  @return SYS_OK 表示完整寄存器读回一致；不一致返回 FRAME_ERROR，通信错误透传。
 *  @note   仅启动阶段、采样前调用；读改写 0x00D bit3，保留其他位，不烧写 EEPROM、不读取角度；失败后方向不确定。
 */
Sys_Status MT6826S_Init(MT6826S *self, MT6826S_Direction direction);
#ifdef __cplusplus
}
#endif

#endif
