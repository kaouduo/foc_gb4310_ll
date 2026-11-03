/**
 * @file bsp_motor.c
 * @brief 板级引脚/通道/量纲参数绑定到静态 porting 实例。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include "bsp_motor.h"
#include "bsp_board_config.h"
#include "motor_sampling_stm32.h"
#include "pwm_output_stm32.h"
static MotorSTM32 motor;
static const MotorSTM32Config config = {
        .pwm = {{BSP_BOARD_PWM_CH1_PORT, BSP_BOARD_PWM_CH1_PIN, BSP_BOARD_PWM_CH1_AF},
                {BSP_BOARD_PWM_CH2_PORT, BSP_BOARD_PWM_CH2_PIN, BSP_BOARD_PWM_CH2_AF},
                {BSP_BOARD_PWM_CH3_PORT, BSP_BOARD_PWM_CH3_PIN, BSP_BOARD_PWM_CH3_AF},
                {BSP_BOARD_PWM_CH1N_PORT, BSP_BOARD_PWM_CH1N_PIN, BSP_BOARD_PWM_CH1N_AF},
                {BSP_BOARD_PWM_CH2N_PORT, BSP_BOARD_PWM_CH2N_PIN, BSP_BOARD_PWM_CH2N_AF},
                {BSP_BOARD_PWM_CH3N_PORT, BSP_BOARD_PWM_CH3N_PIN, BSP_BOARD_PWM_CH3N_AF}},
        .analog = {{BSP_BOARD_CURRENT_1_PORT, BSP_BOARD_CURRENT_1_PIN, 0U},
                   {BSP_BOARD_CURRENT_2_PORT, BSP_BOARD_CURRENT_2_PIN, 0U},
                   {BSP_BOARD_BUS_VOLTAGE_PORT, BSP_BOARD_BUS_VOLTAGE_PIN, 0U}},
        .master_channel = BSP_BOARD_CURRENT_1_CHANNEL,
        .slave_channel = BSP_BOARD_CURRENT_2_CHANNEL,
        .bus_channel = BSP_BOARD_BUS_VOLTAGE_CHANNEL,
        .gpio_clocks = LL_AHB2_GRP1_PERIPH_GPIOA | LL_AHB2_GRP1_PERIPH_GPIOB,
        .timer_hz = 170000000U,
        .pwm_hz = BSP_BOARD_MOTOR_PWM_HZ,
        .deadtime_ns = BSP_BOARD_MOTOR_DEADTIME_NS,
        .irq_priority = BSP_BOARD_MOTOR_IRQ_PRIORITY,
        .u_channel = 1U,
        .v_channel = 3U,
        .w_channel = 2U,
        .duty_min = BSP_BOARD_MOTOR_DUTY_MIN,
        .duty_max = BSP_BOARD_MOTOR_DUTY_MAX,
};
static const MotorScale scale = {
        .adc_reference_v = 3.3f,
        .counts = 4096.0f,
        .current_gain = 20.0f,
        .shunt_ohm = 0.05f,
        .bus_ratio = 8.5f,
        .current_u_sign = -1.0f,
        .current_v_sign = 1.0f,
        .duty_min = BSP_BOARD_MOTOR_DUTY_MIN,
        .duty_max = BSP_BOARD_MOTOR_DUTY_MAX,
        .pwm_hz = BSP_BOARD_MOTOR_PWM_HZ,
};
const MotorHardware BSP_Motor = {
        .context = &motor,
        .scale = &scale,
        .init = MotorSTM32_Init,
        .set_sample_callback = MotorSTM32_SetCallback,
        .start_sampling = MotorSTM32_Start,
        .stop_sampling = MotorSTM32_Stop,
        .read_bus = MotorSTM32_ReadBus,
        .write_duty = PWM_Output_Write,
        .enable_test_output = PWM_Output_EnableTest,
        .enable_output = PWM_Output_Enable,
        .critical_enter = MotorSTM32_CriticalEnter,
        .critical_exit = MotorSTM32_CriticalExit,
        .disable_output = PWM_Output_Disable,
        .snapshot = MotorSTM32_Snapshot,
};
/** @brief 绑定 REV6 静态引脚、量纲与 porting 实例。 */
void BSP_Motor_Bind(void)
{
    MotorSTM32_Bind(&motor, &config);
}
