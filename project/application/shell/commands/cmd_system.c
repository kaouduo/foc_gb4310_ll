/**
 * @file cmd_system.c
 * @brief 系统信息及延后复位、DFU 命令。
 * @author kaouduo
 * @date 2026/10/31
 */
#include <string.h>
#include "app_motor.h"
#include "app_shell.h"
#include "bsp_platform.h"
#include "csh.h"

/** @brief 先停止电机，再请求应答后复位。 */
static int Reboot(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc != 1) {
        csh_printf(shell, "ERR usage: reboot\r\n");
        return -1;
    }
    App_Motor_Stop();
    App_Shell_RequestAction(APP_USB_ACTION_REBOOT);
    return 0;
}
CSH_CMD_EXPORT_ALIAS_FULL(Reboot, reboot, "restart firmware", "reboot\r\n    Reply before resetting firmware.");

/** @brief 先停止电机，再请求应答后进入 ROM DFU。 */
static int Dfu(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc != 1) {
        csh_printf(shell, "ERR usage: dfu\r\n");
        return -1;
    }
    App_Motor_Stop();
    App_Shell_RequestAction(APP_USB_ACTION_DFU);
    return 0;
}
CSH_CMD_EXPORT_ALIAS_FULL(Dfu, dfu, "enter ROM USB DFU", "dfu\r\n    Reply before entering ROM USB DFU.");

/** @brief 输出唯一 ID 或毫秒运行时间。 */
static int System(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc == 2 && strcmp(argv[1], "uid") == 0) {
        uint32_t uid[3];
        (void)BSP_Platform_UniqueId(uid);
        csh_printf(shell, "UID=%08lX%08lX%08lX\r\n", (unsigned long)uid[0], (unsigned long)uid[1],
                   (unsigned long)uid[2]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "time") == 0) {
        csh_printf(shell, "TIME_MS=%lu\r\n", (unsigned long)BSP_Platform.timeMs());
        return 0;
    }
    csh_printf(shell, "ERR usage: system uid|time\r\n");
    return -1;
}
CSH_CMD_EXPORT_ALIAS_FULL(System, system, "system uid|time",
                          "system uid\r\n    Print device UID.\r\nsystem time\r\n    Print uptime in milliseconds.");
