/**
 *  @file   main.c
 *  @brief  完成平台基础初始化、板级 IO 准备及启动任务编排。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  application / 启动编排
 */
#include "FreeRTOS.h"
#include "bsp_internal_flash.h"
#include "bsp_platform.h"
#include "bus_voltage_stm32.h"
#include "current_sampling_stm32.h"
#include "pwm_output_stm32.h"
#include "task.h"
#include "uart_stm32.h"

/* 栈配置以字节填写，传给 FreeRTOS 时换算为 StackType_t 元素数。 */
#define STARTUP_STACK_BYTES 2048U
#define STARTUP_PRIORITY 2U
_Static_assert(STARTUP_STACK_BYTES % sizeof(StackType_t) == 0U, "Startup stack alignment");

static StaticTask_t startupTaskControl;
static StackType_t startupTaskStack[STARTUP_STACK_BYTES / sizeof(StackType_t)];

/** @brief 启动结果供调试器观察；未就绪、成功或具体失败码。 */
volatile Sys_Status appStartupResult = SYS_ERROR_NOT_READY;

/**
 *  @brief  准备调度器启动前的各模块板级 IO。
 *  @param  无。
 *  @return 无返回值；当前各模块 IO 接口不报告错误。
 *  @note   平台时钟和 DWT 初始化成功后调用一次；不等待设备应答或调用任务延时。
 */
static void Board_IO_Init(void)
{
    PWM_Output_BoardIO_Init();
    UART_BoardIO_Init();
    CurrentSampling_BoardIO_Init();
    BusVoltage_BoardIO_Init();
}

/**
 *  @brief  初始化 BSP 资源。
 *  @param  argument 预留启动参数，当前不使用，可为空。
 *  @return 不返回，完成或失败后删除当前启动任务。
 *  @note   调度器启动后执行；保存片内 Flash 资源初始化结果供调试器观察。
 */
static void App_StartupTask(void *argument)
{
    (void)argument;
    appStartupResult = BSP_InternalFlash.init();
    vTaskDelete(NULL);
}

/**
 *  @brief  初始化平台并创建启动任务，随后启动调度器。
 *  @param  无。
 *  @return 不返回，调度器运行；基础初始化或调度失败时进入断言停机。
 *  @note   由芯片启动文件调用一次；main 承担启动模板中 app_main 的职责。
 */
int main(void)
{
    appStartupResult = BSP_Platform_Init();
    if (appStartupResult != SYS_OK) {
        FreeRTOS_AssertFailed(__FILE__, __LINE__);
    }
    Board_IO_Init();
    appStartupResult = SYS_ERROR_NOT_READY;
    (void)xTaskCreateStatic(App_StartupTask, "startup", STARTUP_STACK_BYTES / sizeof(StackType_t), NULL,
                            STARTUP_PRIORITY, startupTaskStack, &startupTaskControl);
    vTaskStartScheduler();
    appStartupResult = SYS_ERROR_IO;
    FreeRTOS_AssertFailed(__FILE__, __LINE__);
}
