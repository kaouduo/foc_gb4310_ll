/**
 * @file app_motor.c
 * @brief 单轴对象组装、IRQ 分派与停机采样/零偏准备。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include <math.h>
#include <string.h>
#include "app_motor_internal.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_platform.h"
#include "foc_math.h"
#include "foc_open_voltage.h"
#include "foc_svpwm.h"

App_MotorContext motor;

/** @brief ADC IRQ 同步入口；只做有界计算，不调用 RTOS、日志、存储或动态内存。 */
static void App_Motor_FastStep(void *context, const MotorSample *sample)
{

    App_MotorContext *self = context;
    if (!self->busy || self->collected >= self->target) {
        return;
    }
    if (sample->flags != MOTOR_SAMPLE_VALID || sample->sequence != self->collected + 1U) {
        ++self->errors;
        self->hardware->disable_output(self->hardware->context);
    }
    uint16_t u = sample->current_u_raw, v = sample->current_v_raw;
    self->sum_u += u;
    self->sum_v += v;
    if (u < self->min_u) {
        self->min_u = u;
    }
    if (u > self->max_u) {
        self->max_u = u;
    }
    if (v < self->min_v) {
        self->min_v = v;
    }
    if (v > self->max_v) {
        self->max_v = v;
    }
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    App_Motor_DiagnosticFastStep(sample);
#endif
    ++self->collected;
}

/** @brief 构造模式、注册采样回调并加载校准参数。 */
Sys_Status App_Motor_Init(void)
{
    motor.hardware = &BSP_Motor;
    const MotorScale *s = BSP_Motor.scale;
    motor.dt_s = 1.0f / s->pwm_hz;
    motor.current_a_per_count = s->adc_reference_v / (s->counts * s->current_gain * s->shunt_ohm);
    FOC_OpenVoltage_Bind(&motor.open);
    Sys_Status status = BSP_Motor.set_sample_callback(BSP_Motor.context, App_Motor_FastStep, &motor);
    motor.initialized = status == SYS_OK;
    return status;
}
/** @brief 立即关闭输出并取消启动；采样由其所有者完成收尾。 */
void App_Motor_Stop(void)
{
    motor.cancel_sampling = true;
    BSP_Motor.disable_output(BSP_Motor.context);
}
/** @brief 读取母线诊断值，运行期间只读监督任务缓存。 */
Sys_Status App_Motor_ReadBus(uint16_t *raw, float *volts)
{
    if (!motor.initialized) {
        return SYS_ERROR_NOT_READY;
    }

    Sys_Status status = BSP_Motor.read_bus(BSP_Motor.context, raw);
    if (status == SYS_OK) {
        *volts = *raw * BSP_Motor.scale->adc_reference_v * BSP_Motor.scale->bus_ratio / BSP_Motor.scale->counts;
    }
    return status;
}
/** @brief 输出关闭下独占采集 1000 点，供生产零偏及诊断使用。 */
Sys_Status App_Motor_Acquire(App_MotorTest test, App_MotorTestResult *out)
{
    if (!motor.initialized) {
        return SYS_ERROR_NOT_READY;
    }
    if (motor.busy) {
        return SYS_ERROR_BUSY;
    }
    if (test > APP_MOTOR_TEST_CHAIN) {
        return SYS_ERROR_PARAM;
    }
    BSP_Motor.disable_output(BSP_Motor.context);
    motor.cancel_sampling = false;
    motor.busy = true;
    memset(out, 0, sizeof(*out));
    motor.test = test;
    motor.target = 1000U;
    motor.collected = motor.errors = motor.encoder_errors = motor.sum_u = motor.sum_v = 0U;
    motor.min_u = motor.min_v = UINT16_MAX;
    motor.max_u = motor.max_v = 0U;
    motor.encoder_max_cycles = motor.control_max_cycles = 0U;
    motor.last_duty = (FOC_Duty){0.5f, 0.5f, 0.5f};
    uint16_t bus_raw;
    Sys_Status status = App_Motor_ReadBus(&bus_raw, &motor.bus_v);
    motor.voltage_limit_v = FOC_VoltageLimit(motor.bus_v, BSP_Motor.scale->duty_min, BSP_Motor.scale->duty_max);
    bool entered = false;
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    if (status == SYS_OK) {
        status = App_Motor_DiagnosticEnter(test, bus_raw, &entered);
    }
#endif
    if (status == SYS_OK) {
        status = BSP_Motor.write_duty(BSP_Motor.context, 0.5f, 0.5f, 0.5f);
    }
    if (status == SYS_OK) {
        status = BSP_Motor.start_sampling(BSP_Motor.context);
    }
    uint32_t start = BSP_Platform.timeMs();
    while (status == SYS_OK && motor.collected < motor.target) {
        if (motor.cancel_sampling) {
            status = SYS_ERROR_BUSY;
            break;
        }
        MotorHardwareSnapshot running;
        BSP_Motor.snapshot(BSP_Motor.context, &running);
        if (running.errors || motor.errors || motor.encoder_errors) {
            status = SYS_ERROR_IO;
            break;
        }
        if (BSP_Platform.timeMs() - start > 250U) {
            status = SYS_ERROR_TIMEOUT;
            break;
        }
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
        if (test == APP_MOTOR_TEST_SYNC) {
            uint16_t raw;
            status = BSP_Motor.read_bus(BSP_Motor.context, &raw);
            if (status != SYS_OK) {
                break;
            }
            ++out->bus_reads;
        }
#endif
        (void)BSP_Platform.delayMs(1U);
    }
    BSP_Motor.disable_output(BSP_Motor.context);
    Sys_Status stop = BSP_Motor.stop_sampling(BSP_Motor.context);
    if (status == SYS_OK) {
        status = stop;
    }
    if (entered) {
        FOC_Exit(&motor.open.base);
    }
    out->encoder_max_cycles = motor.encoder_max_cycles;
    out->control_max_cycles = motor.control_max_cycles;
    out->elapsed_ms = BSP_Platform.timeMs() - start;
    BSP_Motor.snapshot(BSP_Motor.context, &out->hardware);
    out->collected = motor.collected;
    out->callback_errors = motor.errors;
    out->encoder_errors = motor.encoder_errors;
    out->min_u = motor.min_u;
    out->max_u = motor.max_u;
    out->min_v = motor.min_v;
    out->max_v = motor.max_v;
    if (motor.collected) {
        out->mean_u = (float)motor.sum_u / motor.collected;
        out->mean_v = (float)motor.sum_v / motor.collected;
    }
    if (status == SYS_OK && (motor.errors || motor.encoder_errors || out->hardware.errors)) {
        status = SYS_ERROR_IO;
    }
    if (test == APP_MOTOR_TEST_ZERO) {
        motor.zero_valid = status == SYS_OK && out->mean_u > 100.0f && out->mean_u < 3995.0f && out->mean_v > 100.0f &&
                out->mean_v < 3995.0f && (uint32_t)(motor.max_u - motor.min_u) <= 50U &&
                (uint32_t)(motor.max_v - motor.min_v) <= 50U;
        if (motor.zero_valid) {
            motor.offset_u = out->mean_u;
            motor.offset_v = out->mean_v;
        } else if (status == SYS_OK) {
            status = SYS_ERROR_DEVICE;
        }
    }
    const MotorScale *s = BSP_Motor.scale;
    float gain = s->adc_reference_v / (s->counts * s->current_gain * s->shunt_ohm);
    out->offset_u = motor.offset_u;
    out->offset_v = motor.offset_v;
    out->zero_valid = motor.zero_valid;
    out->current_u_a = (out->mean_u - motor.offset_u) * gain * s->current_u_sign;
    out->current_v_a = (out->mean_v - motor.offset_v) * gain * s->current_v_sign;
    if (!motor.zero_valid) {
        out->current_u_a = out->current_v_a = 0.0f;
    }
    out->bus_v = motor.bus_v;
    motor.busy = false;
    return status;
}

/** @brief 监督任务停机采零；不依赖 Shell 或调试诊断入口。 */
Sys_Status App_Motor_PrepareZero(void)
{
    App_MotorTestResult result;
    return App_Motor_Acquire(APP_MOTOR_TEST_ZERO, &result);
}

Sys_Status App_Motor_ReadEncoder(MT6826S_Sample *sample)
{
    return BSP_Encoder_Read(sample);
}
