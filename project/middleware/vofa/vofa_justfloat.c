/**
 * @file vofa_justfloat.c
 * @brief 沿用参考工程逐字节小端格式，使用 memcpy 取得浮点位模式。
 * @author kaouduo
 * @date 2026/11/02
 */
#include "vofa_justfloat.h"
#include <float.h>
#include <string.h>
_Static_assert(sizeof(float) == 4U && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "JustFloat requires IEEE754 binary32");
uint16_t VOFA_JustFloat_Build(const float *channels, uint8_t count, uint8_t *out, uint16_t capacity)
{
    uint16_t length = (uint16_t)VOFA_JUSTFLOAT_FRAME_BYTES(count);
    if (count == 0U || count > VOFA_JUSTFLOAT_MAX_CHANNELS || capacity < length) {
        return 0U;
    }
    for (uint8_t i = 0U; i < count; ++i) {
        uint32_t bits;
        memcpy(&bits, &channels[i], sizeof(bits));
        out[i * 4U] = (uint8_t)bits;
        out[i * 4U + 1U] = (uint8_t)(bits >> 8U);
        out[i * 4U + 2U] = (uint8_t)(bits >> 16U);
        out[i * 4U + 3U] = (uint8_t)(bits >> 24U);
    }
    const uint8_t tail[] = {0x00U, 0x00U, 0x80U, 0x7fU};
    memcpy(out + length - sizeof(tail), tail, sizeof(tail));
    return length;
}
