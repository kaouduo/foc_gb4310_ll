/**
 *  @file   bsp_encoder.h
 *  @brief  提供板载 MT6826S 的单实例初始化与按需读取入口。
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

#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H
#include "mt6826s_drv.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif
/**
 *  @brief  初始化板载 MT6826S 并配置顺时针递增方向。
 *  @param  无。
 *  @return SYS_OK 表示方向寄存器配置及读回一致；其他负值为通信或校验错误。
 *  @note   平台时钟和 DWT 就绪后由启动任务调用一次；读改写方向位并校验，不读角度、不烧写
 * EEPROM；成功不代表磁场和角度反馈正常。
 */
Sys_Status BSP_Encoder_Init(void);
/**
 *  @brief  按需读取板载编码器的校验有效样本。
 *  @param  sample 有效样本缓冲区。
 *  @return SYS_OK 表示本次样本有效；其他负值表示忙、通信或协议错误。
 *  @note   初始化完成后供未来控制中断或 shell 显式调用；失败保留原输出，当前不安排自动采样。
 */
Sys_Status BSP_Encoder_Read(MT6826S_Sample *sample);
/**
 *  @brief  按需读取编码器诊断寄存器。
 *  @param  address 有效的 12 位寄存器地址。
 *  @param  value 有效的字节输出缓冲区。
 *  @return 正常返回 SYS_OK，其他负值为完整事务错误。
 *  @note   供未来 shell 诊断使用；无 CRC，失败不改变输出，控制运行期间的访问归属由业务协调。
 */
Sys_Status BSP_Encoder_ReadRegister(uint16_t address, uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif
