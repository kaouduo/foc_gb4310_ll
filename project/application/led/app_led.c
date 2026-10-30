/**
 *  @file   app_led.c
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

#include "app_led.h"

/**
 *  @brief  根据整机状态计算红灯亮度。
 *  @param  state 整机阶段及锁存故障快照。
 *  @param elapsedMs 当前指示模式已持续的毫秒数。
 *  @return 0 至 1000 的亮度千分比。
 *  @note   纯计算；故障快闪、初始化熄灭、待机呼吸、标定双闪、运行常亮；不修改系统状态，不执行 FOC 保护。
 */
uint16_t App_LED_Brightness(System_State state, uint32_t elapsedMs)
{
    if (state.state == SYSTEM_STATE_FAULT) {
        return elapsedMs % 200U < 100U ? 1000U : 0U;
    }
    if (state.state == SYSTEM_STATE_RUNNING) {
        return 1000U;
    }
    if (state.state == SYSTEM_STATE_CALIBRATING) {
        uint32_t pulse = elapsedMs % 1000U;
        return pulse < 100U || (pulse >= 200U && pulse < 300U) ? 1000U : 0U;
    }
    if (state.state != SYSTEM_STATE_STANDBY) {
        return 0U;
    }
    uint32_t phase = elapsedMs % 2000U;
    uint32_t ramp = phase <= 1000U ? phase : 2000U - phase;
    /* 平方曲线补偿视觉亮度，保持两端连续且不需要浮点运算。 */
    return (uint16_t)((ramp * ramp + 500U) / 1000U);
}
