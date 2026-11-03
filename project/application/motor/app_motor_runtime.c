/**
 * @file app_motor_runtime.c
 * @brief 有界开环运行、母线更新、快速保护与锁存故障。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include <math.h>
#include <string.h>
#include "app_device_state.h"
#include "app_log.h"
#include "app_motor_internal.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_platform.h"
#include "foc_math.h"
#include "foc_transform.h"

#define RUN_BUS_MIN_V 8.0f
#define RUN_BUS_MAX_V 27.0f
#define RUN_CURRENT_LIMIT_A 1.5f
#define RUN_FEEDBACK_MAX_US 5000U

typedef struct {
    volatile App_MotorRunSnapshot view;
    App_MotorOpenCommand command;
    volatile bool owns_sampling, stop_requested;
    bool entered, ramping_down;
    volatile bool finishing; /**< 单一收尾所有者；等待硬件停止时仍阻止启动及重复收尾。 */
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    bool diagnostic_pwm;
    FOC_Duty diagnostic_duty;
#endif
    volatile bool report_pending;
    uint32_t max_age_cycles;
    volatile uint32_t generation, bus_cycles, last_sample_cycles;
    uint32_t previous_sequence, ramp_down_at;
    uint16_t previous_encoder, raw_u, raw_v;
    bool have_encoder;
    MT6826S_Sample encoder;
    Sys_Status encoder_status;
} MotorRuntime;
static MotorRuntime run;

void App_Motor_RuntimeInit(void)
{
    memset(&run, 0, sizeof(run));
    run.encoder_status = SYS_ERROR_NOT_READY;
    run.max_age_cycles = (BSP_Platform_ClockHz() / 1000000U) * RUN_FEEDBACK_MAX_US;
}
bool App_Motor_RuntimeBusy(void)
{
    App_MotorRunState state = run.view.state;
    return run.finishing || run.owns_sampling || (state != APP_MOTOR_IDLE && state != APP_MOTOR_FAULT);
}
bool App_Motor_PreparationCancelled(void)
{
    return run.view.state == APP_MOTOR_PREPARING && run.stop_requested;
}

/** @brief Task/IRQ 共用：先关闭输出，再锁存首现场；不使用 RTOS 或日志。 */
static void Fault(App_MotorReason reason, Sys_Status status, uint32_t faults)
{
    uint32_t mask = BSP_Motor.critical_enter();
    BSP_Motor.disable_output(BSP_Motor.context);
    if (run.view.faults == 0U) {
        run.view.reason = reason;
        run.view.status = status;
        run.view.fault_frame = run.view.frames;
        run.view.fault_raw_u = run.raw_u;
        run.view.fault_raw_v = run.raw_v;
    }
    run.view.faults |= faults;
    run.view.state = APP_MOTOR_FAULT;
    run.report_pending = true;
    App_DeviceState_RaiseFault(faults);
    BSP_Motor.critical_exit(mask);
}

/** @brief 短事务认领收尾；硬件停止等待在全局中断开放时执行，其他任务不得重复收尾。 */
static void Finish(App_MotorReason reason)
{
    uint32_t mask = BSP_Motor.critical_enter();
    BSP_Motor.disable_output(BSP_Motor.context);
    if (run.finishing) {
        BSP_Motor.critical_exit(mask);
        return;
    }
    run.finishing = true;
    bool stop_sampling = run.owns_sampling;
    if (run.view.faults == 0U) {
        run.view.state = APP_MOTOR_STOPPING;
    }
    BSP_Motor.critical_exit(mask);
    /* 硬件先禁止采样 IRQ 再等待 ADC 停止；全局中断保持可用。 */
    Sys_Status status = stop_sampling ? BSP_Motor.stop_sampling(BSP_Motor.context) : SYS_OK;
    mask = BSP_Motor.critical_enter();
    run.owns_sampling = false;
    if (run.entered) {
        FOC_Exit(&motor.open.base);
        run.entered = false;
    }
    if (run.view.faults == 0U) {
        run.view.reason = reason;
        run.view.status = status;
        run.view.state = APP_MOTOR_IDLE;
    }

    run.report_pending = true;
    BSP_Motor.critical_exit(mask);

    if (run.view.faults) {
        run.view.state = APP_MOTOR_FAULT;
    }
    if (status != SYS_OK && run.view.faults == 0U) {
        Fault(APP_MOTOR_REASON_SAMPLE, status, SYSTEM_FAULT_POWER_STAGE);
    }
    if (run.view.faults == 0U) {
        App_DeviceState_Set(SYSTEM_STATE_STANDBY);
    }
    run.finishing = false;
}

/** @brief 取消启动请求并同步收尾已运行的模式。 */
void App_Motor_RequestStop(void)
{
    uint32_t mask = BSP_Motor.critical_enter();
    motor.cancel_sampling = true;
    ++run.generation;
    run.stop_requested = true;
    BSP_Motor.disable_output(BSP_Motor.context);
    App_MotorRunState state = run.view.state;
    if (state == APP_MOTOR_QUEUED) {
        run.view.state = APP_MOTOR_IDLE;
        run.view.reason = APP_MOTOR_REASON_STOP;
    }
    /* 准备期由采集所有者响应取消并停止 ADC；这里仅立即关闭功率输出。 */
    BSP_Motor.critical_exit(mask);
    if (state == APP_MOTOR_OPEN || state == APP_MOTOR_STOPPING || (state == APP_MOTOR_FAULT && run.owns_sampling)) {
        Finish(APP_MOTOR_REASON_STOP);
    }
}

/** @brief 排队提交有限时间开环请求。 */
Sys_Status App_Motor_RequestOpen(const App_MotorOpenCommand *cmd)
{
    if (!isfinite(cmd->frequency_hz) || fabsf(cmd->frequency_hz) > 100.0f || !isfinite(cmd->voltage_v) ||
        cmd->voltage_v <= 0.0f || cmd->duration_ms < 100U || cmd->duration_ms > 10000U || cmd->ramp_ms == 0U ||
        cmd->ramp_ms > cmd->duration_ms / 2U) {
        return SYS_ERROR_PARAM;
    }
    uint32_t mask = BSP_Motor.critical_enter();
    Sys_Status status = SYS_OK;
    if (!motor.initialized || App_DeviceState_Get().faults || run.view.faults) {
        status = SYS_ERROR_NOT_READY;
    } else if (motor.busy || App_Motor_RuntimeBusy()) {
        status = SYS_ERROR_BUSY;
    } else {
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
        run.diagnostic_pwm = false;
#endif
        run.command = *cmd;
        ++run.generation;
        run.stop_requested = false;
        run.view.state = APP_MOTOR_QUEUED;
    }
    BSP_Motor.critical_exit(mask);
    return status;
}

#if FOC_ENABLE_MOTOR_DIAGNOSTICS
Sys_Status App_Motor_RequestPWM(App_MotorTest test)
{
    if (test < APP_MOTOR_TEST_PWM_U || test > APP_MOTOR_TEST_PWM_ALL) {
        return SYS_ERROR_PARAM;
    }
    /* 与普通运行共用排队门禁；短事务防止 supervisor 提前启动。 */
    uint32_t mask = BSP_Motor.critical_enter();
    const App_MotorOpenCommand command = {0.0f, 1.0f, 1000U, 200U};
    Sys_Status status = App_Motor_RequestOpen(&command);
    if (status == SYS_OK) {
        run.diagnostic_pwm = true;
        run.diagnostic_duty = (FOC_Duty){0.5f, 0.5f, 0.5f};
        if (test == APP_MOTOR_TEST_PWM_U || test == APP_MOTOR_TEST_PWM_ALL) {
            run.diagnostic_duty.u = 0.2f;
        }
        if (test == APP_MOTOR_TEST_PWM_V) {
            run.diagnostic_duty.v = 0.2f;
        }
        if (test == APP_MOTOR_TEST_PWM_W || test == APP_MOTOR_TEST_PWM_ALL) {
            run.diagnostic_duty.w = 0.8f;
        }
    }
    BSP_Motor.critical_exit(mask);
    return status;
}
#endif

/** @brief 读取母线、执行保护并发布带时间戳的缓存。 */
static Sys_Status UpdateBus(void)
{
    uint16_t raw;
    Sys_Status status = BSP_Motor.read_bus(BSP_Motor.context, &raw);
    if (status != SYS_OK) {
        Fault(APP_MOTOR_REASON_STALE, status, SYSTEM_FAULT_CONTROL_TIMEOUT);
        return status;
    }
    const MotorScale *scale = BSP_Motor.scale;
    float volts = raw * scale->adc_reference_v * scale->bus_ratio / scale->counts;
    if (volts < RUN_BUS_MIN_V || volts > RUN_BUS_MAX_V) {
        Fault(volts < RUN_BUS_MIN_V ? APP_MOTOR_REASON_UNDER_VOLTAGE : APP_MOTOR_REASON_OVER_VOLTAGE, SYS_ERROR_DEVICE,
              volts < RUN_BUS_MIN_V ? SYSTEM_FAULT_UNDER_VOLTAGE : SYSTEM_FAULT_OVER_VOLTAGE);
        return SYS_ERROR_DEVICE;
    }
    float limit = FOC_VoltageLimit(volts, scale->duty_min, scale->duty_max);
    uint32_t mask = BSP_Motor.critical_enter();
    run.view.bus_raw = raw;
    run.view.bus_v = volts;
    run.view.voltage_limit_v = limit;
    run.bus_cycles = BSP_Platform.dwtCycles();
    ++run.view.bus_updates;
    BSP_Motor.critical_exit(mask);
    return SYS_OK;
}

/** @brief 开环不占用 IRQ 的编码器 SPI；由 supervisor 采集遥测，Shell 只读缓存。 */
static void UpdateEncoder(void)
{
    MT6826S_Sample sample;
    Sys_Status status = BSP_Encoder_Read(&sample);
    uint32_t mask = BSP_Motor.critical_enter();
    run.encoder_status = status;
    run.view.encoder_valid = status == SYS_OK;
    if (status == SYS_OK) {
        if (run.have_encoder) {
            int32_t delta = (int32_t)sample.angleRaw - run.previous_encoder;
            if (delta > (int32_t)MT6826S_COUNTS_PER_TURN / 2) {
                delta -= MT6826S_COUNTS_PER_TURN;
            }
            if (delta < -(int32_t)MT6826S_COUNTS_PER_TURN / 2) {
                delta += MT6826S_COUNTS_PER_TURN;
            }
            run.view.mechanical_delta_counts += delta;
        }
        run.have_encoder = true;
        run.previous_encoder = sample.angleRaw;
        run.encoder = sample;
        run.view.encoder_raw = sample.angleRaw;
    } else {
        ++run.view.encoder_errors;
    }
    BSP_Motor.critical_exit(mask);
}

/** @brief 完成停机零偏准备并在命令仍有效时启用有限输出。 */
static void Start(void)
{
    uint32_t generation = run.generation;
    run.view.state = APP_MOTOR_PREPARING;
    run.view.status = SYS_OK;
    run.view.reason = APP_MOTOR_REASON_NONE;
    run.view.frames = run.view.bus_updates = run.view.encoder_errors = 0U;
    run.view.peak_current_a = run.view.applied_voltage_v = 0.0f;
    run.view.mechanical_delta_counts = 0;
    run.have_encoder = false;
    run.previous_sequence = 0U;
    run.ramping_down = false;
    if (UpdateBus() != SYS_OK) {
        return;
    }
    Sys_Status status = App_Motor_PrepareZero();
    if (generation != run.generation || run.stop_requested) {
        Finish(APP_MOTOR_REASON_STOP);
        return;
    }
    if (status != SYS_OK) {
        Fault(APP_MOTOR_REASON_ZERO, status, SYSTEM_FAULT_POWER_STAGE);
        return;
    }
    if (UpdateBus() != SYS_OK) {
        return;
    }
    UpdateEncoder();

    float volts = run.command.voltage_v < run.view.voltage_limit_v ? run.command.voltage_v : run.view.voltage_limit_v;
    FOC_OpenVoltageCommand command = {run.command.frequency_hz, volts, run.command.ramp_ms * 0.001f};
    status = FOC_OpenVoltage_SetCommand(&motor.open, &command);
    FOC_Input input = {
            .dt_s = motor.dt_s, .bus_voltage_v = run.view.bus_v, .voltage_limit_v = run.view.voltage_limit_v};
    if (status == SYS_OK) {
        status = FOC_Enter(&motor.open.base, &input);
    }
    if (status != SYS_OK) {
        Fault(APP_MOTOR_REASON_COMPUTE, status, SYSTEM_FAULT_INTERNAL);
        return;
    }
    run.entered = true;
    run.view.target_frames = (BSP_Motor.scale->pwm_hz / 1000U) * run.command.duration_ms;
    run.ramp_down_at = run.view.target_frames - (BSP_Motor.scale->pwm_hz / 1000U) * run.command.ramp_ms;
    uint32_t mask = BSP_Motor.critical_enter();
    if (generation != run.generation || run.stop_requested || App_DeviceState_Get().faults) {
        BSP_Motor.critical_exit(mask);
        Finish(APP_MOTOR_REASON_STOP);
        return;
    }
    FOC_Duty initial = {0.5f, 0.5f, 0.5f};
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    if (run.diagnostic_pwm) {
        initial = run.diagnostic_duty;
    }
#endif
    status = BSP_Motor.write_duty(BSP_Motor.context, initial.u, initial.v, initial.w);
    run.last_sample_cycles = BSP_Platform.dwtCycles();
    run.owns_sampling = true;
    if (status == SYS_OK) {
        status = BSP_Motor.start_sampling(BSP_Motor.context);
    }
    if (status == SYS_OK) {
        status = BSP_Motor.enable_output(BSP_Motor.context, run.view.target_frames);
    }
    if (status == SYS_OK) {
        run.view.state = APP_MOTOR_OPEN;
    }
    BSP_Motor.critical_exit(mask);
    if (status != SYS_OK) {
        Fault(APP_MOTOR_REASON_SAMPLE, status, SYSTEM_FAULT_POWER_STAGE);
        return;
    }
    App_DeviceState_Set(SYSTEM_STATE_RUNNING);
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    if (run.diagnostic_pwm) {
        LOG_I("motor", "PWM diagnostic started: duty_milli=%ld,%ld,%ld duration=1000 ms", (long)(initial.u * 1000.0f),
              (long)(initial.v * 1000.0f), (long)(initial.w * 1000.0f));
        return;
    }
#endif
    LOG_I("motor", "Open voltage started: %ld mHz %ld mV duration=%lu ms ramp=%lu ms",
          (long)(run.command.frequency_hz * 1000.0f), (long)(volts * 1000.0f), (unsigned long)run.command.duration_ms,
          (unsigned long)run.command.ramp_ms);
}

bool App_Motor_RuntimeFastStep(const MotorSample *sample)
{
    if (!run.owns_sampling) {
        return false;
    }
    if (run.view.state != APP_MOTOR_OPEN) {
        run.previous_sequence = sample->sequence;
        return true;
    }
    run.last_sample_cycles = sample->completed_cycles;
    run.raw_u = sample->current_u_raw;
    run.raw_v = sample->current_v_raw;
    if (sample->flags != MOTOR_SAMPLE_VALID ||
        (run.previous_sequence != 0U && sample->sequence != run.previous_sequence + 1U)) {
        Fault(APP_MOTOR_REASON_SAMPLE, SYS_ERROR_IO, SYSTEM_FAULT_POWER_STAGE);
        return true;
    }
    run.previous_sequence = sample->sequence;
    if (sample->completed_cycles - run.bus_cycles > run.max_age_cycles) {
        Fault(APP_MOTOR_REASON_STALE, SYS_ERROR_TIMEOUT, SYSTEM_FAULT_CONTROL_TIMEOUT);
        return true;
    }
    if (run.raw_u < 16U || run.raw_v < 16U || run.raw_u > 4079U || run.raw_v > 4079U) {
        Fault(APP_MOTOR_REASON_RAIL, SYS_ERROR_DEVICE, SYSTEM_FAULT_OVER_CURRENT);
        return true;
    }
    const MotorScale *scale = BSP_Motor.scale;
    float u = (run.raw_u - motor.offset_u) * motor.current_a_per_count * scale->current_u_sign;
    float v = (run.raw_v - motor.offset_v) * motor.current_a_per_count * scale->current_v_sign;
    float w = -u - v;
    run.view.current_u_a = u;
    run.view.current_v_a = v;
    run.view.current_w_a = w;
    float peak = fabsf(u) > fabsf(v) ? fabsf(u) : fabsf(v);
    if (fabsf(w) > peak) {
        peak = fabsf(w);
    }
    if (peak > run.view.peak_current_a) {
        run.view.peak_current_a = peak;
    }
    if (peak > RUN_CURRENT_LIMIT_A) {
        Fault(APP_MOTOR_REASON_CURRENT, SYS_ERROR_DEVICE, SYSTEM_FAULT_OVER_CURRENT);
        return true;
    }
    if (++run.view.frames >= run.view.target_frames) {

        BSP_Motor.disable_output(BSP_Motor.context);
        run.view.state = APP_MOTOR_STOPPING;
        run.view.reason = APP_MOTOR_REASON_DONE;
        return true;
    }
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    if (run.diagnostic_pwm) {
        /* 固定占空比已写入；本帧电流、母线时效、ADC 和时长保护仍全部执行。 */
        return true;
    }
#endif
    if (!run.ramping_down && run.view.frames >= run.ramp_down_at) {
        FOC_OpenVoltageCommand command = {run.command.frequency_hz, 0.0f, run.command.ramp_ms * 0.001f};
        (void)FOC_OpenVoltage_SetCommand(&motor.open, &command);
        run.ramping_down = true;
    }

    FOC_Input input = {.dt_s = motor.dt_s,
                       .bus_voltage_v = run.view.bus_v,
                       .voltage_limit_v = run.view.voltage_limit_v,
                       .current_valid = true};
    FOC_Output output;
    Sys_Status status = FOC_Step(&motor.open.base, &input, &output);
    FOC_Duty duty;
    if (status == SYS_OK) {
        float sine, cosine, alpha, beta;
        FOC_SinCos(output.electrical_angle_rad, &sine, &cosine);
        FOC_InversePark(output.ud_v, output.uq_v, sine, cosine, &alpha, &beta);
        status = FOC_SVPWM(alpha, beta, input.bus_voltage_v, &duty);
        if (status == SYS_OK) {
            status = BSP_Motor.write_duty(BSP_Motor.context, duty.u, duty.v, duty.w);
        }
        run.view.applied_voltage_v = output.ud_v;
    }
    if (status != SYS_OK) {
        Fault(APP_MOTOR_REASON_COMPUTE, status, SYSTEM_FAULT_INTERNAL);
    }
    return true;
}

/** @brief 推进准备、保护、校准与停机保存。 */
void App_Motor_Process(void)
{
    if (run.view.state == APP_MOTOR_QUEUED) {
        Start();
    }
    if (run.view.state == APP_MOTOR_OPEN) {
        MotorHardwareSnapshot hw;
        BSP_Motor.snapshot(BSP_Motor.context, &hw);
        /* 先读 ISR 时间戳，再读当前时间。反序会被 IRQ 插入更新，导致无符号差值下溢。 */
        uint32_t last_sample = run.last_sample_cycles;
        uint32_t now = BSP_Platform.dwtCycles();
        if (now - last_sample > run.max_age_cycles) {
            Fault(APP_MOTOR_REASON_STALE, SYS_ERROR_TIMEOUT, SYSTEM_FAULT_CONTROL_TIMEOUT);
        } else if (hw.errors) {
            Fault(APP_MOTOR_REASON_DEADLINE, SYS_ERROR_TIMEOUT, SYSTEM_FAULT_CONTROL_TIMEOUT);
        } else if ((!hw.output_enabled || !hw.sampling) && run.view.frames + 2U < run.view.target_frames) {
            Fault(APP_MOTOR_REASON_SAMPLE, SYS_ERROR_DEVICE, SYSTEM_FAULT_POWER_STAGE);
        } else if (App_DeviceState_Get().faults) {
            Fault(APP_MOTOR_REASON_SAMPLE, SYS_ERROR_DEVICE, SYSTEM_FAULT_POWER_STAGE);
        } else if (UpdateBus() == SYS_OK) {
            UpdateEncoder();
        }
    }
    if (run.view.state == APP_MOTOR_STOPPING ||
        (run.view.state == APP_MOTOR_FAULT && (run.owns_sampling || run.entered))) {
        Finish(run.view.reason);
    }
    if (run.report_pending) {
        uint32_t mask = BSP_Motor.critical_enter();
        run.report_pending = false;
        App_MotorRunSnapshot snapshot = run.view;
        BSP_Motor.critical_exit(mask);
        if (snapshot.faults) {
            LOG_E("motor", "Fault reason=%u status=%d bits=0x%lx frame=%lu raw=%u,%u", (unsigned)snapshot.reason,
                  (int)snapshot.status, (unsigned long)snapshot.faults, (unsigned long)snapshot.fault_frame,
                  snapshot.fault_raw_u, snapshot.fault_raw_v);
        } else {
            LOG_I("motor", "Stopped reason=%u frames=%lu peak_mA=%ld mechanical_delta=%ld counts",
                  (unsigned)snapshot.reason, (unsigned long)snapshot.frames, (long)(snapshot.peak_current_a * 1000.0f),
                  (long)snapshot.mechanical_delta_counts);
        }
    }
}

/** @brief 复制运行或上次停机的遥测快照。 */
void App_Motor_GetRunSnapshot(App_MotorRunSnapshot *snapshot)
{
    uint32_t mask = BSP_Motor.critical_enter();
    *snapshot = run.view;
    BSP_Motor.critical_exit(mask);
}
/** @brief 读取编码器样本，准备或运行时只读缓存。 */
Sys_Status App_Motor_ReadEncoder(MT6826S_Sample *sample)
{
    if (!App_Motor_RuntimeBusy()) {
        return BSP_Encoder_Read(sample);
    }
    uint32_t mask = BSP_Motor.critical_enter();
    Sys_Status status = run.encoder_status;
    if (status == SYS_OK) {
        *sample = run.encoder;
    }
    BSP_Motor.critical_exit(mask);
    return status;
}
/** @brief 在停止且母线有效时显式清除本模块故障。 */
Sys_Status App_Motor_ClearFault(void)
{
    if (App_Motor_RuntimeBusy() || motor.busy) {
        return SYS_ERROR_BUSY;
    }
    uint16_t raw;
    Sys_Status status = BSP_Motor.read_bus(BSP_Motor.context, &raw);
    if (status != SYS_OK) {
        return status;
    }
    float volts = raw * BSP_Motor.scale->adc_reference_v * BSP_Motor.scale->bus_ratio / BSP_Motor.scale->counts;
    if (volts < RUN_BUS_MIN_V || volts > RUN_BUS_MAX_V) {
        return SYS_ERROR_NOT_READY;
    }
    uint32_t mask = BSP_Motor.critical_enter();
    App_DeviceState_ClearFault(run.view.faults);
    run.view.faults = 0U;
    run.view.state = APP_MOTOR_IDLE;
    BSP_Motor.critical_exit(mask);
    return SYS_OK;
}
