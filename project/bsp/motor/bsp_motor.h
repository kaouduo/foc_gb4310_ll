/**
 * @file bsp_motor.h
 * @brief REV6 电机硬件绑定，业务只访问绑定能力。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include "motor_io.h"

#ifdef __cplusplus
extern "C" {
#endif
extern const MotorHardware BSP_Motor;
/**
 * @brief 绑定 REV6 静态引脚、量纲与 porting 实例。
 * @return 无。
 * @note 启动时构造一次，不初始化外设或开启输出。
 */
void BSP_Motor_Bind(void);

#ifdef __cplusplus
}
#endif
#endif
