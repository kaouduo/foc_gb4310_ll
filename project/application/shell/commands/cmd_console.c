/**
 * @file cmd_console.c
 * @brief 终端清理与帮助命令。
 * @author kaouduo
 * @date 2026/10/31
 */
#include "app_shell.h"
#include "csh.h"

static int Clear(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    if (argc != 1) {
        csh_printf(shell, "ERR usage: clear\r\n");
        return -1;
    }
    App_Shell_RequestClear();
    return 0;
}
CSH_CMD_EXPORT_ALIAS_FULL(Clear, clear, "clear screen and history", "clear\r\n    Clear screen and command history.");

static int Help(int argc, char **argv)
{
    if (argc > 2) {
        chry_shell_t *shell = (void *)argv[argc + 1];
        csh_printf(shell, "ERR usage: help [command]\r\n");
        return -1;
    }
    return csh_builtin_help(argc, argv);
}
CSH_SCMD_EXPORT_ALIAS_FULL(Help, help, "display command usage", "help [command]");
