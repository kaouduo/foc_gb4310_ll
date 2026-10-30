/**
 *  @file   app_task.h
 *  @brief  声明 LED 指示任务入口。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  application / task
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 用 LED 指示任务替换无业务作用的系统观察任务。
 */
#ifndef APP_TASK_H
#define APP_TASK_H

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  根据整机状态更新红灯亮度。
 *  @param  argument 预留任务参数，当前不使用，可为空。
 *  @return 不返回，持续执行周期任务。
 *  @note   由 FreeRTOS 任务上下文调用；每 10 ms 更新硬件 PWM 占空比，其余时间阻塞，不执行 FOC 控制。
 */
void App_LEDTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
