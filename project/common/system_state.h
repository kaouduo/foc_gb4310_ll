/**
 *  @file   system_state.h
 *  @brief  定义闭环无刷驱动板的持久设备状态、故障位和快照。
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
 *  2026/10/30 | kaouduo | V1.0 | 增加红灯硬件呼吸指示与独立整机阶段、锁存故障模型。
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>

/** @brief 闭环无刷驱动板持续保留的设备状态，不是接口返回码或一次控制指令。 */
typedef enum {
    SYSTEM_STATE_INITIALIZING = 0, /*!< 从复位开始初始化平台、设备和业务。 */
    SYSTEM_STATE_STANDBY = 1,      /*!< 初始化完成，电机输出关闭，等待明确的控制请求。 */
    SYSTEM_STATE_CALIBRATING = 2,  /*!< 正在执行电机或编码器标定。 */
    SYSTEM_STATE_RUNNING = 3,      /*!< 电机控制正在执行；具体开环、闭环及控制模式由 FOC 维护。 */
    SYSTEM_STATE_FAULT = 4         /*!< 存在锁存故障；清除后回到待机，不自动恢复运行。 */
} System_DeviceState;

/** @brief 整机锁存故障位，可同时记录多个来源，后续 FOC 只发布自己负责的位。 */
typedef enum {
    SYSTEM_FAULT_NONE = 0U,                  /*!< 无锁存故障。 */
    SYSTEM_FAULT_STARTUP = 1U << 0,          /*!< 启动流程失败。 */
    SYSTEM_FAULT_POWER_STAGE = 1U << 1,      /*!< 功率级异常。 */
    SYSTEM_FAULT_ENCODER = 1U << 2,          /*!< 编码器反馈异常。 */
    SYSTEM_FAULT_OVER_CURRENT = 1U << 3,     /*!< 过流。 */
    SYSTEM_FAULT_UNDER_VOLTAGE = 1U << 4,    /*!< 欠压。 */
    SYSTEM_FAULT_OVER_VOLTAGE = 1U << 5,     /*!< 过压。 */
    SYSTEM_FAULT_OVER_TEMPERATURE = 1U << 6, /*!< 过温。 */
    SYSTEM_FAULT_CONTROL_TIMEOUT = 1U << 7,  /*!< 控制周期或指令失效。 */
    SYSTEM_FAULT_STORAGE = 1U << 8,          /*!< 参数存储资源异常。 */
    SYSTEM_FAULT_INTERNAL = 1U << 9          /*!< 内部不可恢复错误。 */
} System_Fault;

/** @brief 一次原子读取获得的整机状态快照。 */
typedef struct {
    System_DeviceState state; /*!< 驱动板当前持续状态；存在锁存故障时为 FAULT。 */
    uint32_t faults;          /*!< System_Fault 位的组合，0 表示无锁存故障。 */
} System_State;

#endif
