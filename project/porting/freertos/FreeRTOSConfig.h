/**
 *  @file   FreeRTOSConfig.h
 *  @brief  配置 Cortex-M4F 内核、静态与动态内存及调试统计。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.2
 *
 *  @par 文件所属
 *  porting / freertos
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 接入 FreeRTOS，将 DWT 测量与内核系统时间分开。
 *  2026/10/31 | kaouduo | V1.2 | 配置动态堆、对象调试及静态定时器服务。
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H /* FreeRTOS 配置头文件保护。 */

#include "stm32g4xx.h"

#define configUSE_PREEMPTION 1                                /* 启用抢占式调度。 */
#define configUSE_TIME_SLICING 1                              /* 同优先级就绪任务轮流使用时间片。 */
#define configCPU_CLOCK_HZ (SystemCoreClock)                  /* 内核时钟频率，单位 Hz。 */
#define configTICK_RATE_HZ 1000U                              /* 系统节拍频率 1000 Hz，每 tick 为 1 ms。 */
#define configMAX_PRIORITIES 7U                               /* 任务优先级共 7 级，取值 0 至 6。 */
#define configMINIMAL_STACK_SIZE 128U                         /* 空闲任务栈为 128 个栈元素，即 512 字节。 */
#define configMAX_TASK_NAME_LEN 16U                           /* 任务名称最多 16 字节，包含结束符。 */
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS /* 使用 32 位 tick 计数。 */
#define configIDLE_SHOULD_YIELD 1                             /* 空闲任务向同优先级就绪任务让出 CPU。 */
#define configUSE_TICKLESS_IDLE 0                             /* 关闭无节拍低功耗模式。 */
#define configSUPPORT_STATIC_ALLOCATION 1                     /* 允许静态创建任务和内核对象。 */
#define configSUPPORT_DYNAMIC_ALLOCATION 1                    /* 允许从 RTOS 堆动态创建任务和对象。 */
/* 与参考 HAL 工程一致，heap_4 管理独立的 16 KB RTOS 堆。 */
#define configTOTAL_HEAP_SIZE (16U * 1024U) /* RTOS 动态堆容量为 16 KB。 */
#define INCLUDE_xTaskGetSchedulerState 1
#define configUSE_MUTEXES 1                 /* 启用带优先级继承的互斥量。 */
#define configUSE_RECURSIVE_MUTEXES 0       /* 关闭递归互斥量。 */
#define configUSE_COUNTING_SEMAPHORES 1     /* 启用计数信号量。 */
#define configQUEUE_REGISTRY_SIZE 8         /* 调试注册表最多保存 8 个内核对象。 */
#define configUSE_TIMERS 1                  /* 启用软件定时器服务。 */
#define configTIMER_TASK_PRIORITY 1U        /* 定时器服务任务优先级为 1。 */
#define configTIMER_QUEUE_LENGTH 8U         /* 定时器命令队列最多保存 8 条命令。 */
#define configTIMER_TASK_STACK_DEPTH 256U   /* 定时器任务栈为 256 个栈元素，即 1024 字节。 */
#define configUSE_NEWLIB_REENTRANT 0        /* 不为每个任务单独分配 Newlib 重入结构。 */
#define configUSE_IDLE_HOOK 0               /* 关闭用户空闲任务钩子。 */
#define configUSE_TICK_HOOK 0               /* 关闭用户 tick 中断钩子。 */
#define configCHECK_FOR_STACK_OVERFLOW 2    /* 启用二级任务栈溢出检查。 */
#define configUSE_MALLOC_FAILED_HOOK 1      /* 动态分配失败时调用故障钩子。 */
#define configUSE_TRACE_FACILITY 1          /* 启用任务及对象调试跟踪字段。 */
#define configRECORD_STACK_HIGH_ADDRESS 1   /* 记录任务栈高地址，供调试器使用。 */
#define configGENERATE_RUN_TIME_STATS 1     /* 启用任务运行时间统计。 */
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() FreeRTOS_RunTimeStats_Init() /* 初始化 DWT 运行时间测量累加器。 */
#define portGET_RUN_TIME_COUNTER_VALUE() FreeRTOS_RunTimeStats_Counter()      /* 读取运行时间统计值，单位 us。 */
#define configCHECK_HANDLER_INSTALLATION 1                                    /* 检查 SVC 与 PendSV 向量安装。 */
#define configPRIO_BITS __NVIC_PRIO_BITS                                      /* NVIC 实现的中断优先级位数。 */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15U                           /* 最低中断优先级，数值为 15。 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U                       /* 内核 API 的中断优先级数值门限为 5。 */
#define configKERNEL_INTERRUPT_PRIORITY                                                                                \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS)) /* SysTick 与 PendSV 使用的编码优先级。 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY                                                                           \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS)) /* 内核 API 的编码优先级门限。 */
#define INCLUDE_vTaskDelay 1                                                 /* 编译任务相对延时接口。 */
#define INCLUDE_vTaskDelete 1                                                /* 编译任务删除接口。 */
#define INCLUDE_xTaskDelayUntil 1                                            /* 编译任务固定周期延时接口。 */
#define INCLUDE_vTaskSuspend 1                                               /* 编译任务挂起与恢复接口。 */
#define INCLUDE_uxTaskGetStackHighWaterMark 1                                /* 编译任务最小剩余栈查询接口。 */

/* 映射到芯片启动文件的异常向量，SysTick 也由原厂移植层独占。 */
#define vPortSVCHandler SVC_Handler         /* 内核 SVC 处理函数映射至启动向量。 */
#define xPortPendSVHandler PendSV_Handler   /* 内核任务切换函数映射至 PendSV 向量。 */
#define xPortSysTickHandler SysTick_Handler /* 内核节拍函数映射至 SysTick 向量。 */

#ifndef __ASSEMBLER__
#ifdef __cplusplus
extern "C" {
#endif
/**
 *  @brief  初始化任务运行时间的 DWT 测量累加器。
 *  @param  无。
 *  @return 无返回值。
 *  @note   仅调度器启动路径调用；平台 DWT 已运行，不重置硬件周期计数器。
 */
void FreeRTOS_RunTimeStats_Init(void);
/**
 *  @brief  读取供内核统计使用的累计执行微秒数。
 *  @param  无。
 *  @return 32 位活动周期换算的微秒累计值，约 71.6 分钟回绕。
 *  @note   仅内核统计路径串行调用；两次采样小于一次 DWT 回绕，不含停止计数的睡眠与调试暂停时间，不是系统时钟。
 */
uint32_t FreeRTOS_RunTimeStats_Counter(void);

/**
 *  @brief  保存断言现场并停止运行。
 *  @param  file 调用处的静态文件名字符串。
 *  @param line 调用处的源码行号。
 *  @return 不返回。
 *  @note   任务、启动或中断上下文均可进入；关闭中断并保留文件和行号供调试，不打印、不分配内存。
 */
void FreeRTOS_AssertFailed(const char *file, unsigned long line);
#ifdef __cplusplus
}
#endif
#define configASSERT(expression)                                                                                       \
    do {                                                                                                               \
        if (!(expression)) {                                                                                           \
            FreeRTOS_AssertFailed(__FILE__, __LINE__);                                                                 \
        }                                                                                                              \
    } while (0) /* 断言失败时保存现场并停机。 */
#endif

#endif
