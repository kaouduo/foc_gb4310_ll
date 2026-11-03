/**
 * @file vofa_justfloat.h
 * @brief 参考工程 JustFloat 组帧，无硬件、任务和发送依赖。
 * @author kaouduo
 * @date 2026/11/02
 */
#ifndef VOFA_JUSTFLOAT_H
#define VOFA_JUSTFLOAT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define VOFA_JUSTFLOAT_MAX_CHANNELS 16U
#define VOFA_JUSTFLOAT_FRAME_BYTES(channels) ((uint32_t)(channels) * 4U + 4U)
/** @brief 编码小端 IEEE754 binary32 数组，追加 00 00 80 7F。
 * @param channels count 个有效浮点值，不与输出重叠；不含帧尾位模式正无穷。
 * @param count 通道数 1..16。
 * @param out 调用者拥有的输出缓冲，不保留指针。
 * @param capacity 容量，字节。
 * @return 编码字节数；通道数或容量错误返回 0，不写输出。
 * @note 纯计算；FOC 遥测仅发布成功计算的有限值。
 */
uint16_t VOFA_JustFloat_Build(const float *channels, uint8_t count, uint8_t *out, uint16_t capacity);
#ifdef __cplusplus
}
#endif
#endif
