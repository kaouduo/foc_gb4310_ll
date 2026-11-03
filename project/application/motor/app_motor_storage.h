/**
 * @file app_motor_storage.h
 * @brief 单页、单记录校准参数序列化，不持久化运行对象。
 *
 * @author kaouduo
 * @date 2026/11/01
 */
#ifndef APP_MOTOR_STORAGE_H
#define APP_MOTOR_STORAGE_H

#include "app_motor_calibration.h"

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 加载版本、CRC 和物理字段均有效的记录。
 * @param calibration 输出参数，失败时清零为无效。
 * @return SYS_OK、FRAME、CRC 或 Flash 读取错误。
 * @note 任务调用，Flash 已初始化，不自动擦写修复。
 */
Sys_Status App_Motor_StorageLoad(MotorCalibration *calibration);
/* 调用方持有电机生命周期，排队请求也不能与保存交错。 */
/**
 * @brief 停机写入单记录并重新加载核对。
 * @param calibration 已通过质量检查且内容稳定的参数。
 * @return SYS_OK、PARAM、BUSY 或擦写/读回错误。
 * @note 调用方独占生命周期；擦页后失败不保证保留旧记录。
 */
Sys_Status App_Motor_StorageSave(const MotorCalibration *calibration);

#ifdef __cplusplus
}
#endif
#endif
