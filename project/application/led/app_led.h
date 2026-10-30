/**
 *  @file   app_led.h
 *  @brief  将整机状态映射为 LED 亮度模式。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  application / led
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 增加红灯硬件呼吸指示与独立整机阶段、锁存故障模型。
 */

#ifndef APP_LED_H
#define APP_LED_H

#include <stdint.h>
#include "system_state.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  根据整机状态计算红灯亮度。
 *  @param  state 整机阶段及锁存故障快照。
 *  @param elapsedMs 当前指示模式已持续的毫秒数。
 *  @return 0 至 1000 的亮度千分比。
 *  @note   纯计算；故障快闪、初始化熄灭、待机呼吸、标定双闪、运行常亮；不修改系统状态，不执行 FOC 保护。
 */
uint16_t App_LED_Brightness(System_State state, uint32_t elapsedMs);

#ifdef __cplusplus
}
#endif

#endif
