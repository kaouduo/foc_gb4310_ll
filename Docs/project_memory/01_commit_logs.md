# 提交记录

更新日期：2026-10-31。本地 `main` 分支，未配置远程，未推送。

1–5 为 REV6 基础工程建立，五个阶段逐段提交，每段提交前都用该段自己的 `CMakeLists.txt` 与 `main.c` 通过 `REV6-Debug` 与 `REV6-Release` 编译，不按目录机械暂存。

6–9 为 CherryUSB CDC 功能。阶段 0–5 的源码是逐阶段累积的同一份工作区状态，中间形态没有单独保存，因此不伪造不可编译的中间提交，改按“依赖固定 / USB 传输层 / 应用与 DFU / 文档”四条职责线提交；`CMakeLists.txt` 的编译输入行按 hunk 分别进入 6 之后的第 7、8 次提交。最终 HEAD 通过 `REV6-Debug` 与 `REV6-Release` 编译。

测试文件 `tests/` 与 `build/` 不入库；FreeRTOS 内核源码不入库（`cmake/freertos.cmake` 固定版本、URL 与 SHA256，首次配置自动下载）。

## 1. build(platform): 建立 REV6 STM32G431 LL 固件基础工程

分层目录、CMSIS/LL 源码与许可证、Cortex-M4F 工具链、Debug/Release、C11/C++17、启动文件、Flash 链接布局、170 MHz HSI 时钟、DWT、REV6 板级引脚、统一返回码、UTF-8 与格式配置。
该段 `main` 只做时钟与 DWT 初始化后空闲。Debug FLASH 1.61%，Release 1.00%。

## 2. feat(platform): 接入 FreeRTOS 启动流程与片内 Flash 服务

固定 FreeRTOS 版本与下载校验、启动任务与资源初始化顺序、内核毫秒计时、静态任务、异常与分配失败钩子、heap_4 与独立 16 KB 堆、运行时间统计与调试信息、片内 Flash 独立实例与任务互斥读写接口。
该段 `main` 创建启动任务后运行调度器，启动任务只初始化 Flash。Debug FLASH 15.31%，Release 9.24%。

## 3. feat(indicator): 增加设备状态管理与红灯指示功能

持久设备状态、故障锁存与显式恢复、LED 基类与 GPIO/PWM 子类、板级实例绑定、待机呼吸/标定双闪/运行常亮/故障快闪、初始化期间熄灭并在发布结果后创建指示任务。
Debug FLASH 19.50%，Release 10.72%。

## 4. feat(encoder): 增加 MT6826S 初始化与按需读取接口

芯片协议与 LL SPI 通信分离、单实例板级绑定、角度解码与 CRC/固定字段/设备状态校验、顺时针方向配置（保留其他寄存器位并读回验证）、完整事务互斥与通信超时、预留控制中断与 shell 读取接口，暂不启动周期采样。
Debug FLASH 21.69%，Release 11.59%。

## 5. docs(project): 补充构建流程与固件接口说明

工程文档收敛到 `Docs/project_memory`：架构与硬件、平台与 FreeRTOS、设备状态与 LED、MT6826S、接口与编码约定、CLion 调试，以及 `.gitlab` MR 模板。
`Docs` 根目录不再保留专题文档，原七份专题文档的结论已并入以上记忆文件。

## 6. build(usb): 固定 CherryUSB v1.6.1 依赖并构建设备库

固定 tag、commit `c9625ffa773ad10b8824d1b5361bca2ccc1f3d1e` 与归档 SHA256，源码不入库、不修改、不格式化；`cmake/cherryusb.cmake` 首次配置自动获取并校验，已有目录核对版本与 23 项源码/头文件/许可证哈希，支持离线配置。
只编译 Device Core、CDC ACM、FSDEV 三个源文件生成 `cherryusb_device`，不编译 Host、其他类、示例与 HAL 胶水；`usb_config.h` 固定单总线、Full Speed、4 字节对齐、256 B 控制请求缓冲、PMA 1 KiB，关闭打印与 EP0 线程。

## 7. feat(usb): 接入 USB 时钟、重新枚举时序与单路 CDC 静态传输

`system_clock` 增加 HSI48 就绪超时、USB 时钟源选择与 CRS SOF 自动校准，主频仍为 170 MHz 且不等待主机 SOF；板级配置固定 PA11/PA12、GPIOA、IRQ 优先级 6、200 ms 控制器复位与 300 ms 数据线拉低、0483/5740 与 ST 默认字符串。
`BSP_USB_CDC` 编排断开时序并把 UID 生成的 24 位十六进制序列号下传描述符；`usb_io` 只操作引脚，`usb_dc_port` 管外设门控、复位、低层钩子与 LP 中断；`usb_cdc`、`usb_descriptors` 提供 64 B 静态收发、NAK 背压、必要 ZLP、会话代次、挂起/恢复同步与错误后的重新配置等待。公共事件类型放 `common/usb_cdc_types.h`，BSP 头不暴露 LL、CherryUSB 或 RTOS 类型。

## 8. feat(usb): 增加 CDC 文本命令与 G431 ROM DFU 交接

新增静态 USB 任务（1024 B 栈、优先级 2）与行解析模块：128 B 正文、CR/LF/CRLF、裸 CR 的 10 ms 行尾判定、迟到配对 LF、超长丢弃恢复、原样回显，以及 `uid`、`reboot`、`dfu` 三条精确匹配命令；响应完成或最多等待 200 ms 后执行平台动作，断开不取消已接受的命令。
`main` 直接调用 `System_Clock_Init`、`DWT_Init`，删除 `BSP_Platform_Init`；`BSP_Platform_EnterDFU` 先停止 CDC 并复用 200/300 ms 断开时序，再由 `system_bootloader` 校验 ROM 向量、清理 SysTick/NVIC、先降频再复位外设与 PLL、切回 HSI、重映射 ROM，最后由独立汇编清 BASEPRI/FAULTMASK/CONTROL 并切换 MSP 跳转 0x1FFF0000，不写 Flash、链接布局或选项字节。
同一提交包含联合验收中发现的收包修正：刚消费完当前分包时先继续尝试读取底层待取数据，确认没有数据后才进入阻塞等待，避免消费 RX 通知后让底层包永久 NAK。

## 9. docs(project): 记录 CherryUSB 实施计划、实板验收与提交记录

补齐文档索引、会话上下文、架构与启动顺序、平台与 RTOS 契约、CLion 的 J-Link 持久会话配置；新增 `08_cherryusb_plan.md`（阶段门禁、文本协议、目录职责）与 `09_usb_validation.md`（基线、逐阶段核验与限制）。
记录实板结果：29 项文本边界、串口重开 20 次、持续插线 reboot 20 次、600.28 秒 / 3,534,960 B 逐字节回显、USB 任务栈最低剩余 736 B、LED 任务栈 1800 B、ROM DFU 下载与恢复、参数页 2048 B 前后一致；人工拔插按用户要求取消，不作为失败或待完成项。
最终资源占用：Debug Flash 54764 B / 42.44%、Release 26824 B / 20.79%，RAM 均 27904 B / 85.16%。

## 后续

FOC 算法、TIM1 完整控制、PWM 使能与死区、硬件保护、ADC 同步采样、20 kHz 控制中断、shell 尚未实现，均需单独提交。
MR 标题沿用 `[Feature] 建立 REV6 LL 固件基础工程`，正文只写修改原因、修改内容、验证。
