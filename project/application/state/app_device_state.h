/**
 *  @file   app_device_state.h
 *  @brief  管理闭环无刷驱动板的业务状态及锁存故障。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  application / state
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 增加红灯硬件呼吸指示与独立整机阶段、锁存故障模型。
 */

#ifndef APP_DEVICE_STATE_H
#define APP_DEVICE_STATE_H

#include "system_state.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  发布驱动板的持续运行状态。
 *  @param  state 非 FAULT 的 System_DeviceState 值，由启动流程或电机业务的状态管理者发布。
 *  @return 无返回值。
 *  @note   调用方保证枚举和状态更新归属；故障锁存期间不接受正常状态覆盖，FAULT 通过 RaiseFault 发布。
 */
void App_DeviceState_Set(System_DeviceState state);
/**
 *  @brief  原子锁存一个或多个整机故障位。
 *  @param  faults 已定义 System_Fault 位的组合，0 表示不改变故障。
 *  @return 无返回值。
 *  @note   任务或普通中断均可调用，不使用 RTOS；各故障源只发布自己负责的位，不能代替 FOC 立即关闭功率输出。
 */
void App_DeviceState_RaiseFault(uint32_t faults);
/**
 *  @brief  显式清除指定的锁存故障位。
 *  @param  faults 经上层确认已恢复的 System_Fault 位组合。
 *  @return 无返回值。
 *  @note   仅由故障恢复管理者在故障源已静止或已确认恢复后调用；LED
 * 任务禁止调用，不自动清除其他故障；最后一个故障清除后回到 STANDBY，禁止自动恢复运行。
 */
void App_DeviceState_ClearFault(uint32_t faults);
/**
 *  @brief  原子读取驱动板当前设备状态和故障快照。
 *  @param  无。
 *  @return 同一个原子字解码得到的 System_State 值，不会拼接不同更新时刻的两个字段。
 *  @note   任务或普通中断可调用；仅发布状态值，不作为 FOC 采样数据或其他共享内存的同步屏障。
 */
System_State App_DeviceState_Get(void);

#ifdef __cplusplus
}
#endif

#endif
