/**
 * @file app_motor_calibration.c
 * @brief 正反机械圈辨识、静态零偏与电流映射质量门槛。
 *
 * @author kaouduo
 * @date 2026/11/01
 */
#include "app_motor_calibration.h"
#include <math.h>
#include <string.h>
#include "foc_math.h"
#define COUNTS_PER_TURN 32768.0f
#define HOLD_MS 500U
#define ALIGN_MS 800U
#define STABLE_MS 100U
#define POINTS 33U
static struct {
    MotorCalSnapshot view;
    uint32_t hz, phase_frame, stall_frame;
    int32_t phase_count, stall_count, stable_min, stable_max;
    float frequency, phase_angle, first_count, first_angle, forward_count, forward_angle, forward_offset, align_delta;
    float sum_count, sum_u, sum_v;
    uint32_t stable_samples, points;
    float x[POINTS], y[POINTS];
} cal;

/** @brief 计算两个角度的最短有符号圆周差。 */
static float Difference(float a, float b)
{
    float d = FOC_WrapAngle(a - b);
    return d > FOC_PI ? d - FOC_TWO_PI : d;
}
/** @brief 拟合有符号极对数并计算电角度残差。 */
static Sys_Status FitTrajectory(const float *x, const float *y, uint32_t count, float *poles, float *rms)
{
    if (count < 16U) {
        return SYS_ERROR_NOT_READY;
    }
    float mx = 0.0f, my = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        if (!isfinite(x[i]) || !isfinite(y[i])) {
            return SYS_ERROR_PARAM;
        }
        mx += x[i];
        my += y[i];
    }
    mx /= count;
    my /= count;
    float xx = 0.0f, xy = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        float d = x[i] - mx;
        xx += d * d;
        xy += d * (y[i] - my);
    }
    if (xx < 1.0f) {
        return SYS_ERROR_NOT_READY;
    }
    *poles = xy / xx;
    float error = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        float d = y[i] - my - *poles * (x[i] - mx);
        error += d * d;
    }
    *rms = sqrtf(error / count);
    return isfinite(*poles) && isfinite(*rms) ? SYS_OK : SYS_ERROR_DEVICE;
}
/** @brief 清空本轮统计并进入初始对齐阶段。 */
void MotorCal_Begin(uint32_t hz)
{
    memset(&cal, 0, sizeof(cal));
    cal.hz = hz;
    cal.view.phase = MOTOR_CAL_ALIGN;
    cal.stable_min = INT32_MAX;
    cal.stable_max = INT32_MIN;
}
/** @brief 将本轮标记为失败或取消，候选参数失效。 */
void MotorCal_Abort(MotorCalError error, Sys_Status status)
{
    cal.view.failed_phase = cal.view.phase;
    cal.view.error = error;
    cal.view.status = status;
    cal.view.phase = error == MOTOR_CAL_ERROR_STOP ? MOTOR_CAL_CANCELLED : MOTOR_CAL_FAILED;
    cal.view.result.valid = false;
    cal.view.result.saved = false;
}
/** @brief 复制本轮校准进度与候选结果。 */
void MotorCal_GetSnapshot(MotorCalSnapshot *s)
{
    *s = cal.view;
}
/** @brief 发布保存及读回结果。 */
void MotorCal_SetSaved(Sys_Status status)
{
    cal.view.status = status;
    cal.view.result.saved = status == SYS_OK;
    cal.view.phase = status == SYS_OK ? MOTOR_CAL_DONE : MOTOR_CAL_FAILED;
    if (status != SYS_OK) {
        cal.view.failed_phase = MOTOR_CAL_SAVING;
        cal.view.error = MOTOR_CAL_ERROR_STORAGE;
    }
}
/** @brief 切换阶段、重置统计并生成电压命令动作。 */
static void Phase(MotorCalPhase phase, float frequency, const MotorCalInput *in, MotorCalAction *out)
{
    cal.view.phase = phase;
    cal.phase_frame = in->frames;
    cal.phase_count = in->mechanical_counts;
    cal.phase_angle = in->electrical_angle_rad;
    cal.stall_frame = in->frames;
    cal.stall_count = in->mechanical_counts;
    cal.frequency = frequency;
    cal.sum_count = cal.sum_u = cal.sum_v = 0.0f;
    cal.stable_samples = 0;
    cal.stable_min = INT32_MAX;
    cal.stable_max = INT32_MIN;
    cal.points = 0;
    out->change_command = true;
    out->frequency_hz = frequency;
    out->voltage_v = MOTOR_CAL_VOLTAGE_V;
}
/** @brief 在保持阶段末 100 ms 累计观测并判断时间结束。 */
static bool Hold(const MotorCalInput *in, uint32_t duration_ms)
{
    uint32_t elapsed = (in->frames - cal.phase_frame) / (cal.hz / 1000U);
    if (elapsed >= duration_ms - STABLE_MS) {
        cal.sum_count += in->mechanical_counts;
        cal.sum_u += in->current_u_a;
        cal.sum_v += in->current_v_a;
        ++cal.stable_samples;
        if (in->mechanical_counts < cal.stable_min) {
            cal.stable_min = in->mechanical_counts;
        }
        if (in->mechanical_counts > cal.stable_max) {
            cal.stable_max = in->mechanical_counts;
        }
    }
    return elapsed >= duration_ms;
}
/** @brief 检查保持窗口样本数和机械计数跨度。 */
static bool Stable(void)
{
    if (!cal.stable_samples) {
        return false;
    }
    cal.view.stable_span_counts = (uint32_t)(cal.stable_max - cal.stable_min);
    return cal.stable_samples >= 50U && cal.view.stable_span_counts <= 32U;
}
/** @brief 检查两路电流与静态电压方向的一致性。 */
static bool CurrentMap(float angle)
{
    float sine, cosine;
    FOC_SinCos(angle, &sine, &cosine);
    float expected_v = -0.5f * cosine + 0.866025404f * sine;
    float u = cal.sum_u / cal.stable_samples, v = cal.sum_v / cal.stable_samples, w = -u - v;
    float ew = -cosine - expected_v;
    float projection = (u * cosine + v * expected_v + w * ew) / 1.5f;
    float energy = u * u + v * v + w * w;
    /* 至少 20 mA，矢量方向误差小于约 23 度；W 是重建量，仅验证两路一致性。 */
    return projection > 0.02f && projection * projection * 1.5f > 0.85f * energy;
}
/** @brief 发布本轮校准失败并返回对应结果码。 */
static Sys_Status Fail(MotorCalError error, Sys_Status status)
{
    MotorCal_Abort(error, status);
    return status;
}
/** @brief 拟合一方向轨迹并计算该圈的零偏圆周均值。 */
static Sys_Status FitLeg(bool reverse)
{
    float p = 0.0f, rms = 0.0f;
    uint32_t first = 0U;
    /* 静止启动的前 100 ms 不用于稳态斜率拟合；完整机械行程仍从启动点计数。 */
    while (first < cal.points && fabsf(cal.y[first]) < FOC_TWO_PI * MOTOR_CAL_FREQUENCY_HZ * 0.1f) {
        ++first;
    }
    Sys_Status status = FitTrajectory(&cal.x[first], &cal.y[first], cal.points - first, &p, &rms);
    if (!reverse) {
        cal.view.result.forward_poles = fabsf(p);
        cal.view.result.forward_rms_rad = rms;
    } else {
        cal.view.result.reverse_poles = fabsf(p);
        cal.view.result.reverse_rms_rad = rms;
    }
    float rounded = floorf(fabsf(p) + 0.5f);
    if (status != SYS_OK || rounded < 1.0f || rounded > MOTOR_CAL_MAX_POLES ||
        fabsf(fabsf(p) - rounded) > MOTOR_CAL_POLE_TOLERANCE || rms > MOTOR_CAL_FIT_RMS_MAX_RAD) {
        return Fail(MOTOR_CAL_ERROR_QUALITY, SYS_ERROR_DEVICE);
    }
    if (!reverse) {
        cal.view.result.pole_pairs = (uint32_t)rounded;
        cal.view.result.direction = p > 0.0f ? 1 : -1;
    } else {
        if ((uint32_t)rounded != cal.view.result.pole_pairs || (p > 0.0f ? 1 : -1) != cal.view.result.direction ||
            fabsf(fabsf(p) - cal.view.result.forward_poles) > MOTOR_CAL_DIRECTION_TOLERANCE) {
            return Fail(MOTOR_CAL_ERROR_QUALITY, SYS_ERROR_DEVICE);
        }
    }
    float sine_sum = 0.0f, cosine_sum = 0.0f;
    float signed_poles = (float)cal.view.result.direction * (float)cal.view.result.pole_pairs;
    float origin = cal.phase_count * FOC_TWO_PI / COUNTS_PER_TURN;
    for (uint32_t i = first; i < cal.points; ++i) {
        float sine, cosine;
        FOC_SinCos(cal.phase_angle + cal.y[i] - signed_poles * (origin + cal.x[i]), &sine, &cosine);
        sine_sum += sine;
        cosine_sum += cosine;
    }
    float offset = FOC_WrapAngle(atan2f(sine_sum, cosine_sum));
    if (!reverse) {
        cal.forward_offset = offset;
    } else {
        float gap = Difference(offset, cal.forward_offset);
        cal.view.directional_gap_rad = fabsf(gap);
        if (cal.view.directional_gap_rad > 0.5f) {
            return Fail(MOTOR_CAL_ERROR_QUALITY, SYS_ERROR_DEVICE);
        }
        cal.view.result.offset_rad = FOC_WrapAngle(cal.forward_offset + gap * 0.5f);
    }
    return SYS_OK;
}
/** @brief 使用一个监督观测推进校准阶段并生成动作。 */
Sys_Status MotorCal_Step(const MotorCalInput *in, MotorCalAction *out)
{
    *out = (MotorCalAction){0};
    cal.view.elapsed_ms = in->frames / (cal.hz / 1000U);
    if (!in->encoder_valid) {
        return Fail(MOTOR_CAL_ERROR_FEEDBACK, SYS_ERROR_IO);
    }
    if (cal.view.elapsed_ms > MOTOR_CAL_MAX_MS) {
        return Fail(MOTOR_CAL_ERROR_TIMEOUT, SYS_ERROR_TIMEOUT);
    }
    uint32_t elapsed = in->frames - cal.phase_frame;
    MotorCalPhase phase = cal.view.phase;
    if (phase == MOTOR_CAL_FORWARD || phase == MOTOR_CAL_REVERSE) {
        int32_t delta = in->mechanical_counts - cal.phase_count;
        uint32_t magnitude = (uint32_t)(delta < 0 ? -delta : delta);
        if (cal.points < POINTS && magnitude >= cal.points * 1024U) {
            cal.x[cal.points] = delta * FOC_TWO_PI / COUNTS_PER_TURN;
            cal.y[cal.points] = cal.frequency * FOC_TWO_PI * (float)elapsed / cal.hz;
            ++cal.points;
        }
        if (in->frames - cal.stall_frame >= cal.hz / 2U) {
            int32_t motion = in->mechanical_counts - cal.stall_count;
            if (motion < 32 && motion > -32) {
                return Fail(MOTOR_CAL_ERROR_STALL, SYS_ERROR_TIMEOUT);
            }
            cal.stall_frame = in->frames;
            cal.stall_count = in->mechanical_counts;
        }
        float poles = phase == MOTOR_CAL_FORWARD ? MOTOR_CAL_MAX_POLES : cal.view.result.pole_pairs;
        uint32_t timeout = (uint32_t)((poles / MOTOR_CAL_FREQUENCY_HZ * 1.25f + 0.5f) * cal.hz);
        if (elapsed > timeout) {
            return Fail(MOTOR_CAL_ERROR_TIMEOUT, SYS_ERROR_TIMEOUT);
        }
        if (magnitude >= 32768U) {
            if (FitLeg(phase == MOTOR_CAL_REVERSE) != SYS_OK) {
                return SYS_ERROR_DEVICE;
            }
            if (phase == MOTOR_CAL_FORWARD) {
                cal.view.forward_counts = delta;
            } else {
                cal.view.reverse_counts = delta;
            }
            cal.align_delta = Difference(cal.first_angle, in->electrical_angle_rad);
            float frequency = cal.align_delta >= 0.0f ? MOTOR_CAL_FREQUENCY_HZ : -MOTOR_CAL_FREQUENCY_HZ;
            Phase(phase == MOTOR_CAL_FORWARD ? MOTOR_CAL_FORWARD_REALIGN : MOTOR_CAL_REVERSE_REALIGN, frequency, in,
                  out);
        }
        return SYS_OK;
    }
    if (phase == MOTOR_CAL_FORWARD_REALIGN || phase == MOTOR_CAL_REVERSE_REALIGN) {
        float traveled = FOC_TWO_PI * MOTOR_CAL_FREQUENCY_HZ * (float)elapsed / cal.hz;
        if (traveled >= fabsf(cal.align_delta)) {
            Phase(phase == MOTOR_CAL_FORWARD_REALIGN ? MOTOR_CAL_FORWARD_HOLD : MOTOR_CAL_REVERSE_HOLD, 0.0f, in, out);
        }
        return SYS_OK;
    }
    if (phase == MOTOR_CAL_MAP_MOVE || phase == MOTOR_CAL_RETURN) {
        if (elapsed >= cal.hz / 6U) {
            Phase(phase == MOTOR_CAL_MAP_MOVE ? MOTOR_CAL_MAP_HOLD : MOTOR_CAL_ZERO_HOLD, 0.0f, in, out);
        }
        return SYS_OK;
    }
    if (phase == MOTOR_CAL_RAMP_DOWN) {
        if (elapsed >= cal.hz / 5U) {
            cal.view.phase = MOTOR_CAL_SAVING;
            out->finished = true;
        }
        return SYS_OK;
    }
    if (!Hold(in, phase == MOTOR_CAL_ALIGN ? ALIGN_MS : HOLD_MS)) {
        return SYS_OK;
    }
    if (!Stable()) {
        return Fail(MOTOR_CAL_ERROR_QUALITY, SYS_ERROR_DEVICE);
    }
    float mean = cal.sum_count / cal.stable_samples;
    if (phase == MOTOR_CAL_ALIGN || phase == MOTOR_CAL_MAP_HOLD) {
        if (!CurrentMap(in->electrical_angle_rad)) {
            return Fail(MOTOR_CAL_ERROR_CURRENT_MAP, SYS_ERROR_DEVICE);
        }
        Phase(phase == MOTOR_CAL_ALIGN ? MOTOR_CAL_MAP_MOVE : MOTOR_CAL_RETURN,
              phase == MOTOR_CAL_ALIGN ? MOTOR_CAL_FREQUENCY_HZ : -MOTOR_CAL_FREQUENCY_HZ, in, out);
    } else if (phase == MOTOR_CAL_ZERO_HOLD) {
        cal.first_count = mean;
        cal.first_angle = in->electrical_angle_rad;
        Phase(MOTOR_CAL_FORWARD, MOTOR_CAL_FREQUENCY_HZ, in, out);
    } else if (phase == MOTOR_CAL_FORWARD_HOLD) {
        cal.forward_count = mean;
        cal.forward_angle = in->electrical_angle_rad;
        Phase(MOTOR_CAL_REVERSE, -MOTOR_CAL_FREQUENCY_HZ, in, out);
    } else if (phase == MOTOR_CAL_REVERSE_HOLD) {
        float gain =
                (float)cal.view.result.direction * (float)cal.view.result.pole_pairs * FOC_TWO_PI / COUNTS_PER_TURN;
        float a = FOC_WrapAngle(cal.first_angle - gain * cal.first_count);
        float b = FOC_WrapAngle(cal.forward_angle - gain * cal.forward_count);
        float c = FOC_WrapAngle(in->electrical_angle_rad - gain * mean);
        float ab = fabsf(Difference(a, b)), ac = fabsf(Difference(a, c)), bc = fabsf(Difference(b, c));
        cal.view.zero_spread_rad = fmaxf(fmaxf(ab, ac), bc);
        /* 同角度两方向停靠的中心抵消静态摩擦迟滞，再与初始零位比较。 */
        float center = FOC_WrapAngle(b + Difference(c, b) * 0.5f);
        cal.view.return_error_rad = fabsf(Difference(center, a));
        if (cal.view.zero_spread_rad > 0.5f || cal.view.return_error_rad > 0.25f) {
            return Fail(MOTOR_CAL_ERROR_QUALITY, SYS_ERROR_DEVICE);
        }
        float offset = cal.view.result.offset_rad;
        cal.view.static_error_rad =
                fmaxf(fmaxf(fabsf(Difference(a, offset)), fabsf(Difference(b, offset))), fabsf(Difference(c, offset)));
        if (cal.view.static_error_rad > 0.75f) {
            return Fail(MOTOR_CAL_ERROR_QUALITY, SYS_ERROR_DEVICE);
        }
        cal.view.result.valid = true;
        Phase(MOTOR_CAL_RAMP_DOWN, 0.0f, in, out);
        out->voltage_v = 0.0f;
    }
    return SYS_OK;
}
