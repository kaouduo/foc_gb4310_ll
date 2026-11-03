/**
 *  @file   main.c
 *  @brief  完成平台基础初始化、板级 IO 准备及启动任务编排。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.5
 *
 *  @par 文件所属
 *  application / 启动编排
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.5 | 一次性启动任务动态创建，自删除后由空闲任务回收资源。
 *  2026/10/30 | kaouduo | V1.2 | 将启动编排集中在 main，设备资源就绪后创建业务任务。
 *  2026/10/31 | kaouduo | V1.4 | 启动 CDC 设备并保留 USB 初始化故障与中断事件。
 *  2026/10/30 | kaouduo | V1.3 | 绑定 LED 基类句柄，启动任务初始化具体子类后创建状态指示任务。
 */
#include "FreeRTOS.h"
#include "app_device_state.h"
#include "app_log.h"
#include "app_motor.h"
#include "app_task.h"
#include "app_usb_task.h"
#include "bsp_encoder.h"
#include "bsp_internal_flash.h"
#include "bsp_led.h"
#include "bsp_motor.h"
#include "bsp_platform.h"
#include "bsp_usb_cdc.h"
#include "dwt.h"
#include "elog_port.h"
#include "system_clock.h"
#include "task.h"
#include "uart_stm32.h"

/* 栈配置以字节填写，传给 FreeRTOS 时换算为 StackType_t 元素数。 */
#define STARTUP_STACK_BYTES 2048U
#define LED_STACK_BYTES 2048U
#define USB_STACK_BYTES 2048U
#define USB_PRIORITY 2U
#define STARTUP_PRIORITY 2U
#define LED_PRIORITY 1U
_Static_assert(STARTUP_STACK_BYTES % sizeof(StackType_t) == 0U, "Startup stack alignment");
_Static_assert(LED_STACK_BYTES % sizeof(StackType_t) == 0U, "System stack alignment");

static const App_LogPlatform logPlatform = {
        .lock = elog_port_output_lock, .unlock = elog_port_output_unlock, .inISR = Elog_Port_InISR};

static Sys_Status motorHardwareResult;
static StaticTask_t usbTaskControl;
static StackType_t usbTaskStack[USB_STACK_BYTES / sizeof(StackType_t)];
static TaskHandle_t usbTaskHandle;
static StaticTask_t ledTaskControl;
static StackType_t ledTaskStack[LED_STACK_BYTES / sizeof(StackType_t)];
/** @brief 启动结果供调试器观察；未就绪、成功或具体失败码。 */
volatile Sys_Status appStartupResult = SYS_ERROR_NOT_READY;

/** @brief 累计 USB 事件位，仅 IRQ 更新，供调试器观察；实际任务唤醒使用通知位。 */
volatile uint32_t appUsbEvents;

/** @brief 在 USB IRQ 累计事件并通知唯一 USB 任务。 */
static void App_USBEvent(uint32_t events, void *context)
{
    (void)context;
    appUsbEvents |= events;
    BaseType_t wake = pdFALSE;
    (void)xTaskNotifyFromISR(usbTaskHandle, events, eSetBits, &wake);
    portYIELD_FROM_ISR(wake);
}

/**
 *  @brief  准备调度器启动前的各模块板级 IO。
 *  @param  无。
 *  @return 无返回值；电机初始化结果保存在 motorHardwareResult，LED 此处仅绑定对象。
 *  @note   平台时钟和 DWT 初始化成功后调用一次；不重复配置系统基础资源；ADC 校准/就绪使用有界 DWT
 * 轮询，不调用任务延时。
 */
static void Board_IO_Init(void)
{
    BSP_Motor_Bind();
    /* 调度前配置安全 GPIO/关闭输出并校准 ADC；不启动采样。 */
    motorHardwareResult = BSP_Motor.init(BSP_Motor.context);
    UART_BoardIO_Init();
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
    Sys_Status encoderResult = BSP_Encoder_Init();
    LOG_I("boot", "Encoder init status=%d", (int)encoderResult);
    if (encoderResult != SYS_OK) {
        LOG_E("encoder", "Initialization failed status=%d", (int)encoderResult);
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_ENCODER);
    }
    Sys_Status flashResult = BSP_InternalFlash.init();
    LOG_I("boot", "Flash init status=%d", (int)flashResult);
    if (flashResult != SYS_OK) {
        LOG_E("flash", "Initialization failed status=%d", (int)flashResult);
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_STORAGE);
    }
    Sys_Status motorResult = motorHardwareResult;
    if (motorResult == SYS_OK) {
        motorResult = App_Motor_Init();
    }
    LOG_I("boot", "Motor hardware bind/init status=%d output=off", (int)motorResult);
    if (motorResult != SYS_OK) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_POWER_STAGE);
    }

    usbTaskHandle = xTaskCreateStatic(App_USBTask, "usb", USB_STACK_BYTES / sizeof(StackType_t), NULL, USB_PRIORITY,
                                      usbTaskStack, &usbTaskControl);
    App_USBOutput_Init(usbTaskHandle);
    Sys_Status usbResult = BSP_USB_CDC_Init(App_USBEvent, NULL);
    if (usbResult == SYS_OK) {
        LOG_I("usb", "CDC initialized");
        (void)xTaskNotify(usbTaskHandle, APP_USB_TASK_START, eSetBits);
    } else {
        App_USBOutput_Stop();
        vTaskDelete(usbTaskHandle);
    }
    if (usbResult != SYS_OK) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_INTERNAL);
    }
    if (encoderResult == SYS_OK && flashResult == SYS_OK && usbResult == SYS_OK && motorResult == SYS_OK) {
        App_DeviceState_Set(SYSTEM_STATE_STANDBY);
    }
    /* 保留首次失败码；故障位同时保存全部失败来源。 */
    appStartupResult = encoderResult != SYS_OK
            ? encoderResult
            : (flashResult != SYS_OK ? flashResult : (motorResult != SYS_OK ? motorResult : usbResult));
    LOG_I("system", "Startup status=%d", (int)appStartupResult);
    LOG_D("system", "Debug logging enabled");
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
    appStartupResult = System_Clock_Init();
    if (appStartupResult == SYS_OK) {
        appStartupResult = DWT_Init();
    }
    if (appStartupResult != SYS_OK) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP);
        FreeRTOS_AssertFailed(__FILE__, __LINE__);
    }
    Elog_Port_Bind(App_USBOutput_LogWrite, BSP_Platform.timeMs);
    if (!App_Log_Init(&logPlatform)) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_INTERNAL);
        FreeRTOS_AssertFailed(__FILE__, __LINE__);
    }
    LOG_I("boot", "Clock and DWT initialized");
    Board_IO_Init();
    LOG_I("boot", "LL board IO initialized");
    appStartupResult = SYS_ERROR_NOT_READY;
    /* 一次性启动任务从 RTOS 堆分配，自删除后由空闲任务回收栈和控制块。 */
    if (xTaskCreate(App_StartupTask, "startup", STARTUP_STACK_BYTES / sizeof(StackType_t), NULL, STARTUP_PRIORITY,
                    NULL) != pdPASS) {
        App_DeviceState_RaiseFault(SYSTEM_FAULT_STARTUP | SYSTEM_FAULT_INTERNAL);
        appStartupResult = SYS_ERROR_IO;
        FreeRTOS_AssertFailed(__FILE__, __LINE__);
    }
    vTaskStartScheduler();
    App_DeviceState_RaiseFault(SYSTEM_FAULT_INTERNAL);
    appStartupResult = SYS_ERROR_IO;
    FreeRTOS_AssertFailed(__FILE__, __LINE__);
}
