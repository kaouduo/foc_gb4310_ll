# 平台服务与 FreeRTOS

更新日期：2026-10-31。以当日源码为准。

## BSP_Platform

`bsp_platform.h` 定义 `BSP_PlatformTypedef`（`timeMs`、`delayMs`、`dwtCycles`、`dwtElapsedUs`、`dwtDelayUs`），`bsp_platform.c` 提供只读实例 `BSP_Platform`。
五个函数指针在编译期静态绑定且非空，运行中不可替换，因此调用侧不再做空指针或重复绑定检查；应用只包含 BSP 头，不包含 porting 头。
系统初始化由 `main` 直接顺序调用 `System_Clock_Init()`、`DWT_Init()`，保留硬件错误及启动故障处理，删除组合接口 `BSP_Platform_Init()`。低频能力 `ClockHz` / `Reset` / `UniqueId` / `EnterDFU` 保留 BSP 独立函数。

阶段 4 新增静态 USB 任务：栈 1024 B、优先级 2；初始化前创建通知目标，任务等待 START 通知后才访问 CDC。EnterDFU 从任务中停止 CDC，复用 BSP_USB_CDC_Prepare 的 200 ms 控制器复位和 300 ms 数据线拉低，再由 porting 清理时钟/中断，以独立汇编切 MSP 跳转 ROM。当前控制业务未启动；未来调用前须停止电机/采样。实测栈水位及完整 DFU 下载验收待确认。

## DWT 与时间

DWT 只提供原始周期、短时测量与微秒忙等待：不累计系统运行时间、不配置 SysTick、不作为业务时钟。
32 位周期计数约 25.26 秒回绕，无符号相减支持跨一次回绕；测量跨度必须小于该值。调试暂停与内核睡眠不计入，HSI 频偏影响换算精度，任务抢占计入结果。

`BSP_Platform.timeMs()` 来自 1 kHz FreeRTOS tick，从调度器启动计时，32 位约 49.7 天回绕；是内核运行时间，不是 RTC 日历时间。
`BSP_Platform.delayMs()` 走 `vTaskDelay`，只允许任务上下文，阻塞时让出 CPU，精度受 tick 相位与调度影响。
SysTick、PendSV、SVC 由 Cortex-M4F 移植层独占。

## 片内 Flash 服务

最后一页 `0x0801F800`（2048 B）为参数区，程序链接区 126 KB。
读允许主 Flash 范围；擦除与编程只允许参数页。编程地址和长度须 8 字节对齐，源数据可不对齐。
实现内部管理静态互斥量、解锁、错误标志、缓存恢复、读回检查与上锁；互斥量忙立即返回 `SYS_ERROR_BUSY`。
调用约束：只能在调度器启动后的普通任务中调用，电机必须停止；同 bank 擦写暂停取指、临界区屏蔽中断，期间 tick 可能丢失。
超时不能取消已开始的硬件擦除；编程不自动擦除，失败可能留下部分记录，CRC/版本/掉电恢复由业务设计。
全 1 双字跳过，不消耗 ECC 编程状态；已编程双字未擦除不能再次编程。启动过程不执行任何 Flash 擦写。
对象：`bsp/platform/bsp_internal_flash.c` 提供 `BSP_InternalFlash.init / read / erasePage / write`。

## FreeRTOS 配置要点

`project/porting/freertos/FreeRTOSConfig.h`，GCC / ARM_CM4F 移植，硬件浮点：

- 1000 Hz tick、32 位计数、抢占 + 同优先级时间片，优先级 0–6（0 留给空闲任务），最大任务名 16 字节；关闭 tickless。
- 静态与动态分配同时开启；heap_4 管理独立 16 KB RTOS 堆，仅任务/启动上下文可用，控制中断禁止分配；保留 `pvPortMalloc` 链接入口使堆实际计入 RAM。
- 堆与链接脚本的 Newlib 堆、C++ `new`/`malloc` 不互通。heap_4 首次分配才初始化空闲链表，首次分配前查询空闲字节可能为 0。
- 启用互斥量、计数信号量、软件定时器（优先级 1、命令队列 8、静态栈 256 元素即 1024 B）；尚未创建业务定时器。
- NVIC 四位均为抢占优先级；SysTick/PendSV 编码优先级 15，系统调用门限 5 —— 只有数值 5–15 的 ISR 可调用 `FromISR` API。
- 开启断言（`FreeRTOS_AssertFailed` 关中断后死循环，保留 `freertosAssertFile`/`freertosAssertLine`）、二级栈溢出检测、分配失败钩子、异常向量安装检查。
- 开启 trace、栈高地址记录、队列注册表 8、运行时间统计；统计用 DWT 相邻周期累加微秒并保留余数（不重置 DWT、不占定时器），相邻采样须小于一次 DWT 回绕，计数约 71.6 分钟回绕。
- 未启用 Newlib 任务重入：多任务日志或标准库调用须在各自适配中串行化。
- 空闲任务、定时器任务内存由 `freertos_hooks.c` 静态提供。

## 任务现状

startup 动态创建；USB、LED 静态创建，控制块与栈为编译期固定存储：

| 任务 | 优先级 | 栈 | 行为 |
|---|---|---|---|
| `startup` | 2 | 2048 B（RTOS 堆） | xTaskCreate 创建；自删除后由空闲任务回收栈/控制块 |
| `usb` | 2 | 2048 B | 等待 START，统一处理 CDC、Shell、日志和平台动作 |
| `led` | 1 | 2048 B | 每 10 ms 按整机状态更新红灯亮度后阻塞 |
| `IDLE` | 0 | 128 元素 | 内核 |
| `Tmr Svc` | 1 | 256 元素 | 内核，无业务定时器 |

新任务：准备独立 `StaticTask_t` 与 `StackType_t` 数组，在 `main.c` 的 `App_StartupTask` 中按依赖顺序 `xTaskCreateStatic`；声明集中在 `application/task/app_task.h`，入口放 `application/task`。
普通延时使用 `vTaskDelay` / `xTaskDelayUntil` / `BSP_Platform.delayMs`；只有短硬件时序才用 `BSP_Platform.dwtDelayUs`。

## 上板检查

观察初始化后红灯呼吸；断言触发时读 `freertosAssertFile` / `freertosAssertLine`。
ELF 中 `SVC_Handler`、`PendSV_Handler`、`SysTick_Handler` 必须是移植层强符号并装入实际向量表。
编译与向量检查通过不等于硬件已验证。CLion 任务视图用法见 [FreeRTOS CLion 调试](03_freertos_clion_debug.md)。


## Shell / 日志配置及实测

main 在时钟/DWT 就绪后绑定输出/时间回调，以 App_LogPlatform 注入锁、解锁、ISR 检测能力，初始化 EasyLogger、记录基础和 LL IO 就绪，再创建 startup。USB 任务创建后只绑定日志队列，不清启动记录；CDC 成功后发送 START。终端就绪依据 DTR 或实际 OUT 数据，首次提示符成功前保留缓存。

一把静态 FreeRTOS 互斥量保护格式化和配置，库内重复加锁关闭。INCLUDE_xTaskGetSchedulerState=1；调度前不等待锁。消费者绑定前只有 main/启动任务生产，不调用调度前队列临界区；绑定后短临界区复制并通知，不等待 CDC。普通输出 100 ms，平台动作确认最多 200 ms。

三轮从静态栈底部连续 0xA5 实测的最小未使用字节：

| 任务 | Debug 第一轮 | Release 第二轮 | Debug 第三轮 |
|---|---:|---:|---:|
| startup | 1260 B | 1280 B | 1260 B |
| usb | 856 B | 1020 B | 816 B |
| led | 1852 B | 1848 B | 1852 B |

满足本阶段 256 B 目标，仍各保留 2048 B 静态任务栈。未进行长时间压力和 FOC 实时性测试；有界拥塞采用启动前预置满队列，实际日志执行拒绝/统计，不能称为长期多生产者压力验证。方法与调试限制见 11_shell_log_validation.md。


## 启动任务分配修正（2026-10-31，当前实现）

按用户要求，一次性 startup 改为 xTaskCreate，移除原 2048 B 静态栈和 104 B 静态控制块；保持栈深 512 个 StackType_t。创建失败记录 STARTUP|INTERNAL 故障、SYS_ERROR_IO 并断言停机，不启动调度器。初始化结束仍 vTaskDelete(NULL)，由空闲任务稍后释放动态栈和控制块回到 heap_4，并非自删除语句返回时立即释放。

16 KB RTOS 堆大小未改，USB/LED 与日志锁继续静态分配。Debug RAM 28808 B（87.92%），Release 28760 B（87.77%），各减少 2152 B；动态 startup 在运行期间消耗既有 RTOS 堆，删除后可复用。先前三轮 startup 静态栈水位属于历史实现，新版尚未烧录/实测动态堆回收。
