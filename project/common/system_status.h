/**
 *  @file   system_status.h
 *  @brief  定义驱动、平台及业务接口共用的操作结果码。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  common
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 实现内部 HSI 最高主频、系统计时及受限 Flash 参数存储。
 */

#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

/** @brief 一次函数调用的结果码，不是持久设备状态；设备状态见 system_state.h。 */
typedef enum {
    SYS_OK = 0,                /*!< 操作已完成。 */
    SYS_ERROR_PARAM = -1,      /*!< 参数或地址范围不合法。 */
    SYS_ERROR_TIMEOUT = -2,    /*!< 硬件状态等待超时。 */
    SYS_ERROR_NOT_READY = -3,  /*!< 时钟或计时模块未就绪。 */
    SYS_ERROR_IO = -4,         /*!< 硬件错误或读回校验失败。 */
    SYS_ERROR_BUSY = -5,       /*!< 当前上下文或资源状态不允许操作。 */
    SYS_ERROR_NOT_ERASED = -6, /*!< 编程目标未处于擦除状态。 */
    SYS_ERROR_CRC = -7,        /*!< 数据 CRC 校验失败。 */
    SYS_ERROR_FRAME = -8,      /*!< 应答帧格式、固定字段或配置读回不符。 */
    SYS_ERROR_DEVICE = -9      /*!< 设备报告异常状态，本次数据不可用于业务。 */
} Sys_Status;

#endif
