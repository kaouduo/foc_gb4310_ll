/**
 * @file app_motor_diagnostics.c
 * @brief 常用停机采样入口与 Debug 专用硬件/算法诊断。
 * @author kaouduo
 * @date 2026/11/01
 */
#include <math.h>
#include <string.h>
#include "app_motor_internal.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_platform.h"
#include "foc_math.h"
#include "foc_transform.h"

#if FOC_ENABLE_MOTOR_DIAGNOSTICS
/** @brief Debug 干运行计算；功率输出保持关闭，IRQ 内禁止日志和 RTOS。 */
void App_Motor_DiagnosticFastStep(const MotorSample *sample)
{
    App_MotorContext *self = &motor;
    uint16_t u = sample->current_u_raw, v = sample->current_v_raw;
    if (self->test == APP_MOTOR_TEST_CHAIN && self->errors == 0U) {
        uint32_t begin = BSP_Platform.dwtCycles();
        MT6826S_Sample encoder;
        if (BSP_Encoder_Read(&encoder) != SYS_OK) {
            ++self->encoder_errors;
        }
        uint32_t encoder_end = BSP_Platform.dwtCycles();
        if (encoder_end - begin > self->encoder_max_cycles) {
            self->encoder_max_cycles = encoder_end - begin;
        }
        const MotorScale *scale = self->hardware->scale;
        float gain = self->current_a_per_count;
        float alpha, beta, id, iq;
        FOC_Clarke((u - self->offset_u) * gain * scale->current_u_sign,
                   (v - self->offset_v) * gain * scale->current_v_sign, &alpha, &beta);
        /* 阶段 1 仅开环干运行；dq 电流在输出角度确定后观测，不伪造标定角度。 */
        float sine, cosine;
        FOC_Input input = {.dt_s = self->dt_s,
                           .bus_voltage_v = self->bus_v,
                           .voltage_limit_v = self->voltage_limit_v,
                           .id_a = 0.0f,
                           .iq_a = 0.0f,
                           .current_valid = true};
        FOC_Output output;
        Sys_Status status = FOC_Step(&self->open.base, &input, &output);
        if (status == SYS_OK) {
            FOC_SinCos(output.electrical_angle_rad, &sine, &cosine);
            FOC_Park(alpha, beta, sine, cosine, &id, &iq);
            FOC_InversePark(output.ud_v, output.uq_v, sine, cosine, &alpha, &beta);
            status = FOC_SVPWM(alpha, beta, self->bus_v, &self->last_duty);
        }
        if (status == SYS_OK) {
            /* 只写预装载，MOE 始终关闭；用于验证绑定和输出寄存器链路。 */
            status = self->hardware->write_duty(self->hardware->context, self->last_duty.u, self->last_duty.v,
                                                self->last_duty.w);
        }
        uint32_t control_cycles = BSP_Platform.dwtCycles() - encoder_end;
        if (control_cycles > self->control_max_cycles) {
            self->control_max_cycles = control_cycles;
        }
        if (status != SYS_OK) {
            ++self->errors;
            self->hardware->disable_output(self->hardware->context);
        }
    }
}
/** @brief 仅链路诊断需要进入开环对象；停机采零不调用模式。 */
Sys_Status App_Motor_DiagnosticEnter(App_MotorTest test, uint16_t bus_raw, bool *entered)
{
    Sys_Status status = SYS_OK;
    if (test == APP_MOTOR_TEST_CHAIN) {
        if (!motor.zero_valid || motor.bus_v < 1.0f || bus_raw >= 4090U) {
            status = SYS_ERROR_NOT_READY;
        } else {
            FOC_Input input = {.dt_s = 1.0f / BSP_Motor.scale->pwm_hz,
                               .bus_voltage_v = motor.bus_v,
                               .voltage_limit_v = FOC_VoltageLimit(motor.bus_v, BSP_Motor.scale->duty_min,
                                                                   BSP_Motor.scale->duty_max)};
            status = FOC_Enter(&motor.open.base, &input);
            *entered = status == SYS_OK;
        }
    }
    return status;
}

/** @brief 已知向量冒烟检查；完整角度枚举和模式夹具留在主机测试。 */
Sys_Status App_Motor_TestAlgorithms(uint32_t *checks)
{
    *checks = 0U;
    float alpha, beta, d, q;
    FOC_Clarke(1.0f, -0.5f, &alpha, &beta);
    if (fabsf(alpha - 1.0f) > 1e-5f || fabsf(beta) > 1e-5f) {
        return SYS_ERROR_IO;
    }
    ++*checks;
    FOC_Park(alpha, beta, 1.0f, 0.0f, &d, &q);
    FOC_InversePark(d, q, 1.0f, 0.0f, &alpha, &beta);
    if (fabsf(alpha - 1.0f) > 1e-5f || fabsf(beta) > 1e-5f) {
        return SYS_ERROR_IO;
    }
    ++*checks;
    FOC_Duty duty;
    if (FOC_SVPWM(0.0f, 0.0f, 24.0f, &duty) != SYS_OK || duty.u != 0.5f || duty.v != 0.5f || duty.w != 0.5f) {
        return SYS_ERROR_IO;
    }
    ++*checks;
    return SYS_OK;
}
#endif

/** @brief 关闭功率输出的采样检查；本阶段不提供带电 PWM 测试。 */
Sys_Status App_Motor_Test(App_MotorTest test, App_MotorTestResult *out)
{
    memset(out, 0, sizeof(*out));
#if !FOC_ENABLE_MOTOR_DIAGNOSTICS
    if (test != APP_MOTOR_TEST_CURRENT && test != APP_MOTOR_TEST_ZERO) {
        return SYS_ERROR_PARAM;
    }
#endif
    return App_Motor_Acquire(test, out);
}
