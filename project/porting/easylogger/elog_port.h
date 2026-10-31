/** @file elog_port.h @brief EasyLogger 平台锁与注入式输出/时间绑定。
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef ELOG_PORT_H
#define ELOG_PORT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* main 在 App_Log_Init 前绑定一次；回调不得记录日志或阻塞 USB。 */
void Elog_Port_Bind(bool (*output)(const char *, size_t), uint32_t (*timeMs)(void));
bool Elog_Port_InISR(void);
void elog_port_output_lock(void);
void elog_port_output_unlock(void);
#ifdef __cplusplus
}
#endif
#endif
