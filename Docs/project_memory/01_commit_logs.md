# 提交记录

更新日期：2026-10-31。本地 `main` 分支，未配置远程，未推送。

五个阶段逐段提交，每段提交前都用该段自己的 `CMakeLists.txt` 与 `main.c` 通过 `REV6-Debug` 与 `REV6-Release` 编译，不按目录机械暂存。
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

## 后续

FOC 算法、TIM1 完整控制、PWM 使能与死区、硬件保护、ADC 同步采样、20 kHz 控制中断、shell 尚未实现，均需单独提交。
MR 标题沿用 `[Feature] 建立 REV6 LL 固件基础工程`，正文只写修改原因、修改内容、验证。
