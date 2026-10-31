/**
 * @file elog_cfg.h
 * @brief EasyLogger 等级、颜色和有界行缓冲配置。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef ELOG_CFG_H
#define ELOG_CFG_H

#define ELOG_OUTPUT_ENABLE
#define ELOG_ASSERT_ENABLE
#if defined(FOC_LOG_DEBUG_BUILD)
#define ELOG_OUTPUT_LVL ELOG_LVL_DEBUG
#else
#define ELOG_OUTPUT_LVL ELOG_LVL_INFO
#endif

#define ELOG_LINE_BUF_SIZE 192
#define ELOG_LINE_NUM_MAX_LEN 5
#define ELOG_FILTER_TAG_MAX_LEN 16
#define ELOG_FILTER_KW_MAX_LEN 16
#define ELOG_FILTER_TAG_LVL_MAX_NUM 1
#define ELOG_NEWLINE_SIGN "\r\n"

#define ELOG_COLOR_ENABLE
#define ELOG_COLOR_ERROR (F_RED B_NULL S_NORMAL)
#define ELOG_COLOR_WARN (F_YELLOW B_NULL S_NORMAL)
#define ELOG_COLOR_INFO (F_GREEN B_NULL S_NORMAL)
#define ELOG_COLOR_DEBUG (F_CYAN B_NULL S_NORMAL)

/* CDC 排队由 USB 任务负责，不启用库内异步、缓冲及 pthread。 */

#endif /* ELOG_CFG_H */
