/**
 *  @file   main.c
 *  @brief  完成平台基础初始化、板级 IO 准备及启动任务编排。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.2
 *
 *  @par 文件所属
 *  application / 启动编排
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.2 | 将启动编排集中在 main，设备资源就绪后创建业务任务。
 */
#include "FreeRTOS.h"
#include "app_device_state.h"
#include "app_task.h"
#include "bsp_internal_flash.h"
#include "bsp_led.h"
#include "bsp_platform.h"
#include "bus_voltage_stm32.h"
#include "current_sampling_stm32.h"
#include "pwm_output_stm32.h"
#include "task.h"
#include "uart_stm32.h"

/* 栈配置以字节填写，传给 FreeRTOS 时换算为 StackType_t 元素数。 */
#define STARTUP_STACK_BYTES 2048U
#define LED_STACK_BYTES 2048U
#define STARTUP_PRIORITY 2U
#define LED_PRIORITY 1U
_Static_assert(STARTUP_STACK_BYTES % sizeof(StackType_t) == 0U, "Startup stack alignment");
_Static_assert(LED_STACK_BYTES % sizeof(StackType_t) == 0U, "System stack alignment");

static StaticTask_t startupTaskControl;
static StackType_t startupTaskStack[STARTUP_STACK_BYTES / sizeof(StackType_t)];
static StaticTask_t ledTaskControl;
static StackType_t ledTaskStack[LED_STACK_BYTES / sizeof(StackType_t)];

/** @brief 启动结果供调试器观察；未就绪、成功或具体失败码。 */
volatile Sys_Status appStartupResult = SYS_ERROR_NOT_READY;

/**
 *  @brief  准备调度器启动前的各模块板级 IO。
 *  @param  无。
 *  @return 无返回值；当前各模块 IO 接口不报告错误，LED 此处仅绑定对象。
 *  @note   平台时钟和 DWT 初始化成功后调用一次；不重复配置系统基础资源，不等待设备应答或调用任务延时。
 */
static void Board_IO_Init(void)
{
    PWM_Output_BoardIO_Init();
    UART_BoardIO_Init();
    CurrentSampling_BoardIO_Init();
    BusVoltage_BoardIO_Init();
    BSP_LED_Bind();
}

/**
 *  @brief  初始化 BSP 资源并创建业务任务。
 *  @param  argument 预留启动参数，当前不使用，可为空。
 *  @return 不返回，完成或失败后删除当前启动任务。
 *  @note   调度器启动后执行；LED 对象已绑定，完成资源初始化并发布待机或故障后创建指示任务。
 */
static void App_StartupTask(void *argument)
{
    (void)argument;
    /* 通过基类执行具体外设初始化；Board_IO_Init 对 LED 只构造绑定对象。 */
    LED_Init(BSP_LED_Red);
    LED_Init(BSP_LED_Green);
    Sys_Status flashResult = BSP_InternalFlash.init();
    if (flashResult != SYS_OK) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_STORAGE);
    }
    if (flashResult == SYS_OK) {
        App_DeviceState_Set(SYSTEM_STATE_STANDBY);
    }
    appStartupResult = flashResult;
    /* 初始化结果发布后才启动指示任务，初始化期间保持驱动默认熄灭。 */
    (void)xTaskCreateStatic(App_LEDTask, "led", LED_STACK_BYTES / sizeof(StackType_t), NULL, LED_PRIORITY, ledTaskStack,
                            &ledTaskControl);
    vTaskDelete(NULL);
}

/**
 *  @brief  初始化平台并创建启动任务，随后启动调度器。
 *  @param  无。
 *  @return 不返回，调度器运行；基础初始化或调度失败时进入断言停机。
 *  @note   由芯片启动文件调用一次；main 承担启动模板中 app_main 的职责，设备与业务资源放在启动任务初始化。
 */
int main(void)
{
    App_DeviceState_Set(SYSTEM_STATE_INITIALIZING);
    appStartupResult = BSP_Platform_Init();
    if (appStartupResult != SYS_OK) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP);
        FreeRTOS_AssertFailed(__FILE__, __LINE__);
    }
    Board_IO_Init();
    appStartupResult = SYS_ERROR_NOT_READY;
    (void)xTaskCreateStatic(App_StartupTask, "startup", STARTUP_STACK_BYTES / sizeof(StackType_t), NULL,
                            STARTUP_PRIORITY, startupTaskStack, &startupTaskControl);
    vTaskStartScheduler();
    App_DeviceState_RaiseFault(SYSTEM_FAULT_INTERNAL);
    appStartupResult = SYS_ERROR_IO;
    FreeRTOS_AssertFailed(__FILE__, __LINE__);
}
