/**
 * @file cmd_motor.c
 * @brief 开环电压请求、状态和显式故障清除；不直接访问端点。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include <string.h>
#include "app_motor.h"
#include "bsp_motor.h"
#include "csh.h"
/* 为上游未标注的接口补充本文件格式检查，不修改第三方源文件。 */
int csh_printf(chry_shell_t *shell, const char *format, ...) __attribute__((format(printf, 2, 3)));

static const char *const states[] = {"idle", "queued", "preparing", "open", "stopping", "fault"};
static const char *const reasons[] = {"none",    "done",     "stop",          "zero",         "sample",
                                      "rail",    "current",  "under_voltage", "over_voltage", "stale",
                                      "compute", "deadline", "calibration"};
static const char *const calPhases[] = {
        "idle",      "align",           "map_move",     "map_hold", "return",          "zero_hold",
        "forward",   "forward_realign", "forward_hold", "reverse",  "reverse_realign", "reverse_hold",
        "ramp_down", "saving",          "done",         "failed",   "cancelled"};
static const char *const calErrors[] = {"none",        "stop",    "feedback", "stall",  "timeout",
                                        "current_map", "quality", "storage",  "runtime"};
static long Milli(float value)
{
    return (long)(value * 1000.0f + (value >= 0.0f ? 0.5f : -0.5f));
}
/* 有界十进制解析，避免 strtof 引入大块 libc/堆分配；支持符号和最多 6 位小数。 */
/** @brief 解析有界十进制数，支持符号和最多六位小数。 */
static bool Float(const char *text, float *value)
{
    bool negative = false, dot = false, have_digit = false;
    if (*text == '-' || *text == '+') {
        negative = *text == '-';
        ++text;
    }
    float result = 0.0f, fraction = 0.1f;
    unsigned digits = 0U, decimals = 0U;
    for (; *text; ++text) {
        if (*text == '.' && !dot) {
            dot = true;
            continue;
        }
        if (*text < '0' || *text > '9' || ++digits > 12U) {
            return false;
        }
        have_digit = true;
        if (dot) {
            if (++decimals > 6U) {
                return false;
            }
            result += (*text - '0') * fraction;
            fraction *= 0.1f;
        } else {
            result = result * 10.0f + (*text - '0');
        }
    }
    *value = negative ? -result : result;
    return have_digit;
}
/** @brief 解析 0～10000 ms 无符号十进制值。 */
static bool Milliseconds(const char *text, uint32_t *value)
{
    uint32_t result = 0U;
    if (!*text) {
        return false;
    }
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        result = result * 10U + (uint32_t)(*text - '0');
        if (result > 10000U) {
            return false;
        }
    }
    *value = result;
    return true;
}
/** @brief 解析电机命令，提交请求或打印一致快照。 */
static int Motor(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc == 3 && strcmp(argv[1], "cal") == 0) {
        if (strcmp(argv[2], "start") == 0) {
            Sys_Status status = App_Motor_RequestCalibration();
            csh_printf(shell, "%s motor cal %s status=%d\r\n", status == SYS_OK ? "OK" : "ERR",
                       status == SYS_OK ? "queued" : "rejected", (int)status);
            return status == SYS_OK ? 0 : -1;
        }
        if (strcmp(argv[2], "stop") == 0) {
            App_Motor_Stop();
            csh_printf(shell, "OK motor stopped\r\n");
            return 0;
        }
        if (strcmp(argv[2], "status") == 0) {
            MotorCalibration c;
            MotorCalSnapshot p;
            App_Motor_GetCalibration(&c, &p);
            csh_printf(shell,
                       "CAL phase=%s error=%s status=%d elapsed_ms=%lu forward_counts=%ld reverse_counts=%ld\r\n",
                       calPhases[p.phase], calErrors[p.error], (int)p.status, (unsigned long)p.elapsed_ms,
                       (long)p.forward_counts, (long)p.reverse_counts);
            csh_printf(shell, "CAL failed_at=%s stable_span_counts=%lu zero_spread_mrad=%ld\r\n",
                       calPhases[p.failed_phase], (unsigned long)p.stable_span_counts, Milli(p.zero_spread_rad));
            csh_printf(shell, "CAL directional_gap_mrad=%ld static_error_mrad=%ld return_error_mrad=%ld\r\n",
                       Milli(p.directional_gap_rad), Milli(p.static_error_rad), Milli(p.return_error_rad));
            csh_printf(shell, "CAL stored valid=%u saved=%u poles=%lu direction=%ld offset_mrad=%ld\r\n", c.valid,
                       c.saved, (unsigned long)c.pole_pairs, (long)c.direction, Milli(c.offset_rad));
            csh_printf(shell,
                       "CAL candidate valid=%u saved=%u poles=%lu direction=%ld offset_mrad=%ld fit_milli=%ld,%ld "
                       "rms_mrad=%ld,%ld\r\n",
                       p.result.valid, p.result.saved, (unsigned long)p.result.pole_pairs, (long)p.result.direction,
                       Milli(p.result.offset_rad), Milli(p.result.forward_poles), Milli(p.result.reverse_poles),
                       Milli(p.result.forward_rms_rad), Milli(p.result.reverse_rms_rad));
            return 0;
        }
    }
    if (argc == 2 && strcmp(argv[1], "status") == 0) {
        App_MotorRunSnapshot s;
        MotorHardwareSnapshot hw;
        App_Motor_GetRunSnapshot(&s);
        BSP_Motor.snapshot(BSP_Motor.context, &hw);
        csh_printf(shell,
                   "MOTOR state=%s last_status=%d faults=0x%08lX reason=%s frames=%lu/%lu output=%u sampling=%u\r\n",
                   states[s.state], (int)s.status, (unsigned long)s.faults, reasons[s.reason], (unsigned long)s.frames,
                   (unsigned long)s.target_frames, hw.output_enabled, hw.sampling);
        csh_printf(shell,
                   "RUN bus_mV=%ld limit_mV=%ld applied_mV=%ld peak_mA=%ld current_mA=%ld,%ld,%ld bus_updates=%lu\r\n",
                   Milli(s.bus_v), Milli(s.voltage_limit_v), Milli(s.applied_voltage_v), Milli(s.peak_current_a),
                   Milli(s.current_u_a), Milli(s.current_v_a), Milli(s.current_w_a), (unsigned long)s.bus_updates);
        csh_printf(
                shell,
                "ENC cached=%u raw=%u delta_counts=%ld errors=%lu; IRQ max_cycles=%lu budget_min=%lu hw_errors=%lu\r\n",
                s.encoder_valid, s.encoder_raw, (long)s.mechanical_delta_counts, (unsigned long)s.encoder_errors,
                (unsigned long)hw.irq_max_cycles, (unsigned long)hw.deadline_min_cycles, (unsigned long)hw.errors);
        if (s.faults) {
            csh_printf(shell, "FAULT frame=%lu raw=%u,%u\r\n", (unsigned long)s.fault_frame, s.fault_raw_u,
                       s.fault_raw_v);
        }
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        App_Motor_Stop();
        csh_printf(shell, "OK motor stopped\r\n");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "clear") == 0) {
        Sys_Status status = App_Motor_ClearFault();
        csh_printf(shell, "%s motor clear status=%d\r\n", status == SYS_OK ? "OK" : "ERR", (int)status);
        return status == SYS_OK ? 0 : -1;
    }
    if ((argc == 2 || argc == 6) && strcmp(argv[1], "open") == 0) {
        App_MotorOpenCommand command = {2.0f, 1.0f, 3000U, 200U};
        if (argc == 6 &&
            (!Float(argv[2], &command.frequency_hz) || !Float(argv[3], &command.voltage_v) ||
             !Milliseconds(argv[4], &command.duration_ms) || !Milliseconds(argv[5], &command.ramp_ms))) {
            goto usage;
        }
        Sys_Status status = App_Motor_RequestOpen(&command);
        csh_printf(shell, "%s motor open %s status=%d\r\n", status == SYS_OK ? "OK" : "ERR",
                   status == SYS_OK ? "queued" : "rejected", (int)status);
        return status == SYS_OK ? 0 : -1;
    }
usage:
    csh_printf(shell,
               "ERR usage: motor open [Hz V duration_ms ramp_ms] | stop | status | clear | cal start|status|stop\r\n");
    return -1;
}
CSH_CMD_EXPORT_ALIAS_FULL(Motor, motor, "开环电压运行、停止、状态和故障清除",
                          "motor open                         - 默认 2 Hz、1 V、3 s、200 ms 上下电压斜坡\r\n"
                          "motor open Hz V duration_ms ramp_ms - 电频率 ±100 Hz；100..10000 ms；斜坡不超过时长一半\r\n"
                          "                                     电压按实际母线及调制范围动态限幅，无固定 8 V 上限\r\n"
                          "motor cal start                    - 2 Hz/5 V 编码器校准，质量通过后停机保存\r\n"
                          "motor cal status                   - 校准阶段、质量结果和已保存参数\r\n"
                          "motor cal stop                     - 取消校准，保留原参数\r\n"
                          "motor stop                         - 立即关闭输出，取消正在准备的启动\r\n"
                          "motor status                       - 运行结果、电流、母线、编码器与故障现场\r\n"
                          "motor clear                        - 条件恢复后显式清故障，不自动重启\r\n");
