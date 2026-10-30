/**
 *  @file   freertos_hooks.c
 *  @brief  提供 FreeRTOS 静态内存、断言和栈溢出处理。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  porting / freertos
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 接入 FreeRTOS，将 DWT 测量与内核系统时间分开。
 */

#include "FreeRTOS.h"
#include "dwt.h"
#include "task.h"

static StaticTask_t idleTaskControl;
static StackType_t idleTaskStack[configMINIMAL_STACK_SIZE];

/* 断言现场保留在 RAM，便于调试器读取，不依赖 printf 或堆。 */
const char *volatile freertosAssertFile;
volatile unsigned long freertosAssertLine;

/**
 *  @brief  向内核提供空闲任务的静态存储。
 *  @param  control 内核接收静态任务控制块地址的位置。
 *  @param stack 内核接收任务栈地址的位置。
 *  @param depth 内核接收栈深度的位置，单位为 StackType_t 元素数。
 *  @return 无返回值。
 *  @note   仅由内核启动路径调用，参数保证非空；存储具备整个固件运行期生命周期，不使用堆。
 */
void vApplicationGetIdleTaskMemory(StaticTask_t **control, StackType_t **stack, configSTACK_DEPTH_TYPE *depth)
{
    *control = &idleTaskControl;
    *stack = idleTaskStack;
    *depth = configMINIMAL_STACK_SIZE;
}

/**
 *  @brief  保存断言现场并停止运行。
 *  @param  file 调用处的静态文件名字符串。
 *  @param line 调用处的源码行号。
 *  @return 不返回。
 *  @note   任务、启动或中断上下文均可进入；关闭中断并保留文件和行号供调试，不打印、不分配内存。
 */
void FreeRTOS_AssertFailed(const char *file, unsigned long line)
{
    __disable_irq();
    freertosAssertFile = file;
    freertosAssertLine = line;
    while (1) {
        __NOP();
    }
}

/**
 *  @brief  处理任务栈溢出。
 *  @param  task 触发检测的任务句柄。
 *  @param name 内核提供的任务名，本接口不解引用。
 *  @return 不返回，进入断言停机。
 *  @note   由内核上下文调用；栈可能已经损坏，不执行打印、阻塞或动态分配。
 */
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    FreeRTOS_AssertFailed(__FILE__, __LINE__);
}

static uint32_t runTimePreviousCycles;
static uint32_t runTimeMicroseconds;
static uint32_t runTimeRemainder;
static uint32_t runTimeCyclesPerUs;
/**
 *  @brief  初始化任务运行时间的 DWT 测量累加器。
 *  @param  无。
 *  @return 无返回值。
 *  @note   调度器启动前由内核调用一次；平台 DWT 已运行，不重置 DWT，保持其他模块的测量起点有效。
 */
void FreeRTOS_RunTimeStats_Init(void)
{
    runTimePreviousCycles = DWT_Cycles();
    runTimeMicroseconds = 0U;
    runTimeRemainder = 0U;
    runTimeCyclesPerUs = SystemCoreClock / 1000000U;
}
/**
 *  @brief  将相邻 DWT 周期间隔累加为内核统计微秒数。
 *  @param  无。
 *  @return 32 位累计微秒数，约 71.6 分钟回绕。
 *  @note   仅内核串行统计路径调用；tick 切换提供常规采样，两次调用必须小于约 25.26 秒；不是系统时间戳。
 */
uint32_t FreeRTOS_RunTimeStats_Counter(void)
{
    uint32_t now = DWT_Cycles();
    uint32_t elapsed = now - runTimePreviousCycles;
    runTimePreviousCycles = now;
    /* 保留不足一微秒的周期，避免频繁任务切换导致截断误差持续累积。 */
    runTimeMicroseconds += elapsed / runTimeCyclesPerUs;
    runTimeRemainder += elapsed % runTimeCyclesPerUs;
    runTimeMicroseconds += runTimeRemainder / runTimeCyclesPerUs;
    runTimeRemainder %= runTimeCyclesPerUs;
    return runTimeMicroseconds;
}
/**
 *  @brief  处理 FreeRTOS 动态堆分配失败。
 *  @param  无。
 *  @return 不返回，进入断言停机并保留现场。
 *  @note   由 pvPortMalloc 失败路径调用；不递归分配、不打印、不阻塞，延续工程现有断言处理策略。
 */
void vApplicationMallocFailedHook(void) { FreeRTOS_AssertFailed(__FILE__, __LINE__); }

static StaticTask_t timerTaskControl;
static StackType_t timerTaskStack[configTIMER_TASK_STACK_DEPTH];

/**
 *  @brief  向内核提供软件定时器服务任务的静态存储。
 *  @param  control 内核接收静态任务控制块地址的位置。
 *  @param  stack 内核接收任务栈地址的位置。
 *  @param  depth 接收栈元素数，当前为 256 个 StackType_t，即 1024 字节。
 *  @return 无返回值。
 *  @note   仅内核启动路径调用；参数由内核保证有效，不创建业务定时器，不从动态堆分配任务栈。
 */
void vApplicationGetTimerTaskMemory(StaticTask_t **control, StackType_t **stack, configSTACK_DEPTH_TYPE *depth)
{
    *control = &timerTaskControl;
    *stack = timerTaskStack;
    *depth = configTIMER_TASK_STACK_DEPTH;
}
