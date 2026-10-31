# 架构、硬件与构建

更新日期：2026-10-31。以当日源码为准；与源码冲突时先看源码。

## 六层结构

依赖方向单向向下：`application → bsp → driver → porting → middleware`，`common` 被各层共用，只有头文件。

| 层 | 目录 | 职责 | 禁止 |
|---|---|---|---|
| application 应用 | `project/application` | `main` 启动编排、整机状态管理 `state/`、灯效映射 `led/`、业务任务 `task/` | 业务任务直接调用 LL 或 porting；main 启动编排允许调用 porting |
| bsp 板级支持 | `project/bsp` | 板级参数、实例绑定、对外薄接口；`platform`、`internal_flash`、`encoder`、`led` | 被 porting 反向引用 |
| driver 器件驱动 | `project/driver` | 与 MCU 无关的协议/抽象：`mt6826s`、`led` 基类 | 引用 LL、BSP 或 porting |
| porting 平台移植 | `project/porting` | LL/HAL 实现：`stm32g4`、`freertos`、`internal_flash`、`pwm_output`、`mt6826s`(SPI)、`uart`、`current_sampling`、`adc_regular`、`led` | 反向引用 BSP；直接改写板级参数 |
| common 公共 | `project/common` | `system_status.h` 返回码、`system_state.h` 状态与故障位、`storage_layout.h` 布局 | 放任何实现 |
| middleware 中间件 | `project/middleware/third_party` | CMSIS、STM32G4xx LL、FreeRTOS 内核（不入库） | 修改或格式化第三方源码 |

## 边界决策

- `main` 是启动编排入口，允许直接调用 porting 的系统和板级 IO 初始化；其他 application 任务/业务模块通过 BSP 访问硬件。此规则由用户明确，不将 main 的调用视为分层例外。

- 片内 Flash 属于平台资源，但对外的 `BSP_InternalFlash` 是**独立实例**，不放进 `BSP_Platform` 能力表。
- 整机设备状态属于业务，放 `application/state`；`common/system_state.h` 只定义类型，BSP 不管业务状态。
- `BSP_Platform` 用只读结构体在编译期绑定 porting 函数指针，常见能力不写转发层；低频能力（频率查询、复位、UID）保留独立函数。
- 返回码只有一份 `Sys_Status`（`common/system_status.h`），driver、porting、BSP、application 直接使用，不逐层复制枚举或别名。器件功能枚举（如旋转方向）仍由驱动自己定义。

## 硬件资源（仅 REV6）

STM32G431CBU6，内部 HSI 16 MHz 经 PLL（M=4、N=85、R=2）到 **170 MHz**。
升频前开启 Range 1 Boost、Flash 4 等待周期，经 AHB 二分频中间态后恢复一分频。

| 功能 | 引脚 |
|---|---|
| 三相主输出 TIM1 | PA8 / PA9 / PA10 |
| TIM1 互补输出 | PB13 / PB14 / PB15 |
| 编码器 SPI1 | PA5 SCK、PA6 MISO、PA7 MOSI，PC4 软件片选 |
| 串口 USART3 | PC10 TX、PC11 RX |
| 电流采样 | PA2 → ADC1、PA4 → ADC2 |
| 母线电压 | PB11 → ADC1 |
| 红灯 | PB6，TIM4_CH1 AF2 PWM，低电平亮 |
| 绿灯 | PB7，普通 GPIO，低电平亮 |

Flash 128 KB，`0x0801F800` 起 2 KB 为参数区，代码链接区 126 KB；RAM 32 KB。

## 启动顺序

`main` 发布 INITIALIZING → `System_Clock_Init` → `DWT_Init` → `Board_IO_Init`（PWM/UART/电流/母线 IO，并 `BSP_LED_Bind`）→ 静态创建启动任务 → `vTaskStartScheduler`。
调度器启动后 `App_StartupTask` 依次 `LED_Init`、`BSP_Encoder_Init`、`BSP_InternalFlash.init`，再创建静态 USB 任务（先等待启动通知）并调用 `BSP_USB_CDC_Init`。分别锁存编码器/存储/USB 故障，全部成功发布 STANDBY；USB 初始化成功后通知其任务开始收发，失败则删除 USB 任务；随后创建 LED 任务并删除自身。
`main` 承担启动模板中 `app_main` 的职责，不再单独保留 `app_main.c`。
初始化期间保持灯灭；初始化失败不进入待机，但仍创建 LED 任务用于故障指示。

## 构建

`CMakePresets.json` 提供 `REV6-Debug`（`-O0 -g3`）与 `REV6-Release`（`-Os -g0`）；工具链参数集中在 `cmake/gcc-arm-none-eabi.cmake`（Cortex-M4 Thumb、硬浮点 ABI、C11/C++17）。
C++ 禁用异常、RTTI、线程安全局部静态初始化——这不表示已适配完整标准库动态运行时。
`cmake/firmware.cmake` 负责链接脚本依赖、hex/bin 输出与尺寸打印；`cmake/freertos.cmake` 固定 FreeRTOS 版本、URL、SHA256 并链接 heap_4。
FreeRTOS 内核源码不入库：首次配置缺目录时自动下载并校验，之后可离线构建。
阶段 5 当前占用：Debug FLASH 54764 B / 42.44%，Release 26824 B / 20.79%，RAM 均 27904 B / 85.16%（含 16 KB RTOS 堆、静态任务栈与链接器最低堆栈预留，不是运行时栈余量）。

FreeRTOS 源码固定 V11.3.2；CMSIS 与 LL 随源码入库，不动、不格式化。
