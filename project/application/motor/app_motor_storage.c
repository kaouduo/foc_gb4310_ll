/**
 * @file app_motor_storage.c
 * @brief 固定 64 字节记录，CRC32，最后提交 8 字节有效标记。
 *
 * @author kaouduo
 * @date 2026/11/01
 */
#include "app_motor_storage.h"
#include <math.h>
#include <string.h>
#include "bsp_internal_flash.h"
#include "bsp_motor.h"
#include "foc_math.h"
#include "storage_layout.h"
#define MAGIC 0x33434f46UL
#define COMMIT0 0x43414c31UL
#define COMMIT1 0xbcbeb3ceUL
/* 版本绑定当前 U/V ADC 极性、增益、编码器分辨率和 PWM U1/V3/W2，改变映射须升级。 */
#define BOARD_SCHEMA 0x47343101UL
_Static_assert(STORAGE_PARAMETER_SIZE >= STORAGE_MOTOR_CALIBRATION_SIZE && STORAGE_MOTOR_CALIBRATION_START % 8U == 0U,
               "Calibration record alignment");
/** @brief 计算记录前 48 B 的反射 IEEE CRC32。 */
static uint32_t CRC(const uint32_t *data)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_MAX;
    for (uint32_t i = 0; i < 48U; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((0U - (crc & 1U)) & 0xedb88320U);
        }
    }
    return ~crc;
}
/** @brief 检查参数的有限性、范围及质量门槛。 */
static bool Valid(const MotorCalibration *c)
{
    return c->valid && c->pole_pairs >= 1U && c->pole_pairs <= MOTOR_CAL_MAX_POLES &&
            (c->direction == 1 || c->direction == -1) && isfinite(c->offset_rad) && c->offset_rad >= 0.0f &&
            c->offset_rad < FOC_TWO_PI && isfinite(c->forward_poles) && isfinite(c->reverse_poles) &&
            fabsf(c->forward_poles - c->pole_pairs) <= MOTOR_CAL_POLE_TOLERANCE &&
            fabsf(c->reverse_poles - c->pole_pairs) <= MOTOR_CAL_POLE_TOLERANCE &&
            fabsf(c->forward_poles - c->reverse_poles) <= MOTOR_CAL_DIRECTION_TOLERANCE &&
            isfinite(c->forward_rms_rad) && isfinite(c->reverse_rms_rad) && c->forward_rms_rad >= 0.0f &&
            c->reverse_rms_rad >= 0.0f && c->forward_rms_rad <= MOTOR_CAL_FIT_RMS_MAX_RAD &&
            c->reverse_rms_rad <= MOTOR_CAL_FIT_RMS_MAX_RAD;
}
/** @brief 将校准参数序列化为 16 个 32 位字。 */
static void Encode(const MotorCalibration *c, uint32_t *w)
{
    memset(w, 0, STORAGE_MOTOR_CALIBRATION_SIZE);
    w[0] = MAGIC;
    w[1] = STORAGE_MOTOR_CALIBRATION_VERSION;
    w[2] = STORAGE_MOTOR_CALIBRATION_SIZE;
    w[3] = c->pole_pairs;
    memcpy(&w[4], &c->direction, 4U);
    memcpy(&w[5], &c->offset_rad, 4U);
    memcpy(&w[6], &c->forward_poles, 4U);
    memcpy(&w[7], &c->reverse_poles, 4U);
    memcpy(&w[8], &c->forward_rms_rad, 4U);
    memcpy(&w[9], &c->reverse_rms_rad, 4U);
    w[10] = BOARD_SCHEMA;
    w[12] = CRC(w);
    w[14] = COMMIT0;
    w[15] = COMMIT1;
}
/** @brief 加载版本、CRC 和物理字段均有效的记录。 */
Sys_Status App_Motor_StorageLoad(MotorCalibration *c)
{
    uint32_t w[16];
    memset(c, 0, sizeof(*c));
    Sys_Status status = BSP_InternalFlash.read(STORAGE_MOTOR_CALIBRATION_START, w, sizeof(w));
    if (status != SYS_OK) {
        return status;
    }
    if (w[0] != MAGIC || w[1] != STORAGE_MOTOR_CALIBRATION_VERSION || w[2] != STORAGE_MOTOR_CALIBRATION_SIZE ||
        w[10] != BOARD_SCHEMA || w[11] || w[13] || w[14] != COMMIT0 || w[15] != COMMIT1) {
        return SYS_ERROR_FRAME;
    }
    if (w[12] != CRC(w)) {
        return SYS_ERROR_CRC;
    }
    MotorCalibration decoded = {.pole_pairs = w[3], .valid = true, .saved = true};
    memcpy(&decoded.direction, &w[4], 4U);
    memcpy(&decoded.offset_rad, &w[5], 4U);
    memcpy(&decoded.forward_poles, &w[6], 4U);
    memcpy(&decoded.reverse_poles, &w[7], 4U);
    memcpy(&decoded.forward_rms_rad, &w[8], 4U);
    memcpy(&decoded.reverse_rms_rad, &w[9], 4U);
    if (!Valid(&decoded)) {
        return SYS_ERROR_FRAME;
    }
    *c = decoded;
    return SYS_OK;
}
/** @brief 停机写入单记录并重新加载核对。 */
Sys_Status App_Motor_StorageSave(const MotorCalibration *c)
{
    if (!Valid(c)) {
        return SYS_ERROR_PARAM;
    }
    MotorHardwareSnapshot hw;
    BSP_Motor.snapshot(BSP_Motor.context, &hw);
    if (hw.output_enabled || hw.sampling) {
        return SYS_ERROR_BUSY;
    }
    uint32_t w[16];
    Encode(c, w);
    Sys_Status status = BSP_InternalFlash.erasePage(STORAGE_PARAMETER_START);
    if (status == SYS_OK) {
        status = BSP_InternalFlash.write(STORAGE_MOTOR_CALIBRATION_START, w, 56U);
    }
    if (status == SYS_OK) {
        status = BSP_InternalFlash.write(STORAGE_MOTOR_CALIBRATION_START + 56U, &w[14], 8U);
    }
    MotorCalibration readback;
    if (status == SYS_OK) {
        status = App_Motor_StorageLoad(&readback);
    }
    if (status == SYS_OK) {
        uint32_t verify[16];
        Encode(&readback, verify);
        if (memcmp(w, verify, sizeof(w)) != 0) {
            status = SYS_ERROR_IO;
        }
    }
    return status;
}
