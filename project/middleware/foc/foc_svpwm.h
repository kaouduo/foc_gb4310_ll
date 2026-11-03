/**
 * @file foc_svpwm.h
 * @brief 零序注入 SVPWM，不访问硬件。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_SVPWM_H
#define FOC_SVPWM_H

#include "system_status.h"

#ifdef __cplusplus
extern "C" {

#endif
typedef struct {
    float u; /**< 逻辑 U 相占空比，范围 [0, 1]。 */
    float v; /**< 逻辑 V 相占空比，范围 [0, 1]。 */
    float w; /**< 逻辑 W 相占空比，范围 [0, 1]。 */
} FOC_Duty;

/**
 * @brief 将 αβ 电压转换为逻辑三相占空比。
 * @param alpha_v α 轴电压，单位 V。
 * @param beta_v β 轴电压，单位 V。
 * @param bus_v 正母线电压，单位 V。
 * @param duty 成功时写入 [0, 1] 占空比。
 * @return SYS_OK；非法数值或越界返回 PARAM，不修改输出。
 * @note 调用前完成电压圆形限幅；板级通道映射归 porting。
 */
Sys_Status FOC_SVPWM(float alpha_v, float beta_v, float bus_v, FOC_Duty *duty);

#ifdef __cplusplus
}
#endif
#endif
