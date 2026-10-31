/** @file cmd_log.c @brief 仅 Debug 注册运行时日志控制命令。
 * @author kaouduo
 * @date 2026/10/31
 */
#include <string.h>
#include "app_log.h"
#include "csh.h"
static int Log(int argc, char **argv)
{
    chry_shell_t *shell = (void *)argv[argc + 1];
    static const char *const names[] = {"off", "error", "warn", "info", "debug"};
    if (argc == 2 && strcmp(argv[1], "level") == 0) {
        csh_printf(shell, "LOG level=%s\r\n", names[App_Log_GetStats().level]);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "color") == 0) {
        csh_printf(shell, "LOG color=%s\r\n", App_Log_GetStats().color ? "on" : "off");
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "level") == 0) {
        for (unsigned i = 0U; i < 5U; ++i) {
            if (strcmp(argv[2], names[i]) == 0) {
                App_Log_SetLevel((App_LogLevel)i);
                csh_printf(shell, "OK log level %s\r\n", names[i]);
                return 0;
            }
        }
    } else if (argc == 3 && strcmp(argv[1], "color") == 0 &&
               (strcmp(argv[2], "on") == 0 || strcmp(argv[2], "off") == 0)) {
        App_Log_SetColor(strcmp(argv[2], "on") == 0);
        csh_printf(shell, "OK log color %s\r\n", argv[2]);
        return 0;
    } else if (argc == 2 && strcmp(argv[1], "stats") == 0) {
        App_LogStats s = App_Log_GetStats();
        csh_printf(shell, "LOG level=%s color=%s emitted=%lu filtered=%lu truncated=%lu dropped=%lu\r\n",
                   names[s.level], s.color ? "on" : "off", (unsigned long)s.emitted, (unsigned long)s.filtered,
                   (unsigned long)s.truncated, (unsigned long)s.dropped);
        return 0;
    }
    csh_printf(shell, "ERR usage: log level off|error|warn|info|debug; log color on|off; log stats\r\n");
    return -1;
}
static const char logDescription[] = "日志等级、颜色与统计控制";
static const char logUsage[] = "log level [off|error|warn|info|debug]  - 查询或设置日志等级，off 关闭后续日志\r\n"
                               "log color [on|off]                   - 查询或设置日志颜色\r\n"
                               "log stats                            - 查看日志统计\r\n"
                               "  emitted   - 已生成并交给输出的记录数（包含入队失败）\r\n"
                               "  filtered  - 运行时被过滤的次数\r\n"
                               "  truncated - 正文过长而截断的记录数\r\n"
                               "  dropped   - 队列拒绝或连接异常作废的记录数";
CSH_CMD_EXPORT_ALIAS_FULL(Log, log, logDescription, logUsage);
