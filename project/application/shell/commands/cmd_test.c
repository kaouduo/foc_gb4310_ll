/**
 * @file cmd_test.c
 * @brief 按需执行单次编码器读取，不启动周期采样。
 * @author kaouduo
 * @date 2026/10/31
 */
#include <string.h>
#include "bsp_encoder.h"
#include "csh.h"

static int Test(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc != 2 || strcmp(argv[1], "enc") != 0) {
        csh_printf(shell, "ERR usage: test enc\r\n");
        return -1;
    }
    MT6826S_Sample sample;
    Sys_Status status = BSP_Encoder_Read(&sample);
    if (status != SYS_OK) {
        csh_printf(shell, "ERR enc status=%d\r\n", (int)status);
        return -1;
    }
    uint32_t angle = ((uint32_t)sample.angleRaw * 36000U + MT6826S_COUNTS_PER_TURN / 2U) / MT6826S_COUNTS_PER_TURN;
    csh_printf(shell, "ENC raw=%u/%u angle_deg=%lu.%02lu status=0x%02X\r\n", (unsigned)sample.angleRaw,
               (unsigned)MT6826S_COUNTS_PER_TURN, (unsigned long)(angle / 100U), (unsigned long)(angle % 100U),
               (unsigned)sample.status);
    return 0;
}
CSH_CMD_EXPORT_ALIAS_FULL(Test, test, "test enc", "test enc\r\n    Read one MT6826S sample; no periodic sampling.");
