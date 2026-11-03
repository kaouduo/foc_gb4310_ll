/**
 * @file cmd_test.c
 * @brief 硬件与算法诊断；运行中读缓存并拒绝冲突测试。
 * @author kaouduo
 * @date 2026/10/31
 */
#include <string.h>
#include "app_log.h"
#include "app_motor.h"
#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "csh.h"

/** @brief 输出一次编码器值，运行时使用缓存。 */
static int TestEncoder(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc != 2 || strcmp(argv[1], "enc") != 0) {
        csh_printf(shell, "ERR usage: test enc\r\n");
        return -1;
    }
    MT6826S_Sample sample;
    Sys_Status status = App_Motor_ReadEncoder(&sample);
    if (status != SYS_OK) {
        csh_printf(shell, "ERR enc status=%d\r\n", (int)status);
        return -1;
    }
    LOG_D("encoder", "raw=%u status=0x%02X", (unsigned)sample.angleRaw, (unsigned)sample.status);
    uint32_t angle = ((uint32_t)sample.angleRaw * 36000U + MT6826S_COUNTS_PER_TURN / 2U) / MT6826S_COUNTS_PER_TURN;
    csh_printf(shell, "ENC raw=%u/%u angle_deg=%lu.%02lu status=0x%02X\r\n", (unsigned)sample.angleRaw,
               (unsigned)MT6826S_COUNTS_PER_TURN, (unsigned long)(angle / 100U), (unsigned long)(angle % 100U),
               (unsigned)sample.status);
    return 0;
}

/* 使用整数格式输出物理量，不引入 newlib 的浮点 printf。 */
static long Milli(float value)
{
    return (long)(value * 1000.0f + (value >= 0.0f ? 0.5f : -0.5f));
}
/** @brief 解析硬件诊断命令并输出统计。 */
static int Test(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc == 2 && strcmp(argv[1], "enc") == 0) {
        return TestEncoder(argc, argv);
    }
    if (argc == 2 && strcmp(argv[1], "bus") == 0) {
        uint16_t raw = 0U;
        float volts = 0.0f;
        Sys_Status status = App_Motor_ReadBus(&raw, &volts);
        csh_printf(shell, "BUS status=%d raw=%u voltage_mV=%ld\r\n", (int)status, (unsigned)raw, Milli(volts));
        return status == SYS_OK ? 0 : -1;
    }
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    if (argc == 2 && strcmp(argv[1], "foc") == 0) {
        uint32_t checks;
        Sys_Status status = App_Motor_TestAlgorithms(&checks);
        csh_printf(shell, "FOC status=%d checks=%lu hardware=untouched\r\n", (int)status, (unsigned long)checks);
        return status == SYS_OK ? 0 : -1;
    }
#endif
    if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        App_Motor_Stop();
        csh_printf(shell, "OK motor output off\r\n");
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "motor") == 0) {
        MotorHardwareSnapshot s;
        BSP_Motor.snapshot(BSP_Motor.context, &s);
        csh_printf(shell, "MOTOR init=%u sampling=%u output=%u timer_hz=%lu arr=%lu samples=%lu errors=%lu\r\n",
                   s.initialized, s.sampling, s.output_enabled, (unsigned long)s.timer_hz,
                   (unsigned long)s.period_counts, (unsigned long)s.samples, (unsigned long)s.errors);
        return 0;
    }
    App_MotorTest test;
    if (argc == 2 && strcmp(argv[1], "current") == 0) {
        test = APP_MOTOR_TEST_CURRENT;
    } else if (argc == 2 && strcmp(argv[1], "zero") == 0) {
        test = APP_MOTOR_TEST_ZERO;
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    } else if (argc == 2 && strcmp(argv[1], "sync") == 0) {
        test = APP_MOTOR_TEST_SYNC;
    } else if (argc == 2 && strcmp(argv[1], "chain") == 0) {
        test = APP_MOTOR_TEST_CHAIN;
#endif
    } else {
        goto usage;
    }
    App_MotorTestResult r;
    Sys_Status status = App_Motor_Test(test, &r);
    if (status == SYS_ERROR_NOT_READY || status == SYS_ERROR_BUSY) {
        csh_printf(shell, "ERR motor status=%d (%s)\r\n", (int)status,
                   status == SYS_ERROR_BUSY ? "motor busy" : "check initialization, zero and bus");
        return -1;
    }
    csh_printf(
            shell, "TEST status=%d samples=%lu cb_errors=%lu hw_errors=%lu enc_errors=%lu elapsed_ms=%lu output=%u\r\n",
            (int)status, (unsigned long)r.collected, (unsigned long)r.callback_errors, (unsigned long)r.hardware.errors,
            (unsigned long)r.encoder_errors, (unsigned long)r.elapsed_ms, r.hardware.output_enabled);
    if (test < APP_MOTOR_TEST_PWM_U) {
        csh_printf(shell,
                   "ADC U=%ldmcount [%u,%u] V=%ldmcount [%u,%u] zero=%u offsets=%ld,%ldmcount current_mA=%ld,%ld "
                   "bus_mV=%ld\r\n",
                   Milli(r.mean_u), r.min_u, r.max_u, Milli(r.mean_v), r.min_v, r.max_v, r.zero_valid,
                   Milli(r.offset_u), Milli(r.offset_v), Milli(r.current_u_a), Milli(r.current_v_a), Milli(r.bus_v));
    } else {
        csh_printf(shell, "PWM last_current_mA=%ld,%ld bus_mV=%ld zero=%u\r\n", Milli(r.current_u_a),
                   Milli(r.current_v_a), Milli(r.bus_v), r.zero_valid);
    }
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
    csh_printf(shell, "SYNC interval_cycles=%lu..%lu irq_max_cycles=%lu CCR(U,V,W)=%lu,%lu,%lu\r\n",
               (unsigned long)r.hardware.interval_min_cycles, (unsigned long)r.hardware.interval_max_cycles,
               (unsigned long)r.hardware.irq_max_cycles, (unsigned long)r.hardware.compare_u,
               (unsigned long)r.hardware.compare_v, (unsigned long)r.hardware.compare_w);
    csh_printf(shell,
               "CHAIN encoder_max_cycles=%lu control_max_cycles=%lu adc_errors=%lu deadline_errors=%lu "
               "budget_min_cycles=%lu bus_reads=%lu\r\n",
               (unsigned long)r.encoder_max_cycles, (unsigned long)r.control_max_cycles,
               (unsigned long)r.hardware.adc_errors, (unsigned long)r.hardware.deadline_errors,
               (unsigned long)r.hardware.deadline_min_cycles, (unsigned long)r.bus_reads);
#endif
    return status == SYS_OK ? 0 : -1;
usage:
    csh_printf(shell,
               "ERR usage: test enc|bus|current|zero|motor|stop"
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
               "|sync|chain|foc"
#endif
               "\r\n");
    return -1;
}
CSH_CMD_EXPORT_ALIAS_FULL(Test, test, "电机硬件链路与 FOC 算法检查",
                          "test enc      - 单次编码器读取\r\n"
                          "test bus      - 母线原始值与毫伏值\r\n"
                          "test current  - 关闭输出采集两相电流；先 zero 才有有效零偏\r\n"
                          "test zero     - 输出关闭、无电流时采集 RAM 零偏，不写 Flash\r\n"
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
                          "test sync     - 1000 次同步采样及并行母线读取、IRQ 耗时检查\r\n"
                          "test chain    - 编码器/变换/开环/SVPWM/CCR 全链路，功率输出关闭\r\n"
                          "test foc      - Clarke/Park/SVPWM 已知向量冒烟检查\r\n"
#endif
                          "test motor    - 硬件初始化、采样和输出状态\r\n"
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
#endif
                          "test stop     - 关闭输出并停止采样\r\n");
