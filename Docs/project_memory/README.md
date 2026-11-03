# REV6 LL 工程长期记录

更新日期：2026-10-31（Asia/Taipei）。记录本次对话收敛后的约束和当前源码快照，不逐条记录临时修正。

阅读顺序：

1. [对话工作与工程约束](02_session_context.md)：工程位置、长期规则、未完成边界、验证状态。
2. [架构、硬件与构建](04_architecture.md)：六层职责与边界、引脚、启动顺序、构建参数。
3. [平台服务与 FreeRTOS](05_platform_rtos.md)：BSP_Platform、DWT、时间、Flash 服务、内核配置与任务。
4. [设备状态、LED 与 MT6826S](06_devices_business.md)：状态机、灯效、编码器协议。
5. [接口与编码约定](07_interface_rules.md)：防御检查边界、C/C++ 混合、提交规范。
6. [FreeRTOS CLion 调试](03_freertos_clion_debug.md)：RTOS 任务视图为空的原因与断点位置。
7. [提交记录](01_commit_logs.md)：实际提交标题与内容。
8. [CherryUSB 实施计划](08_cherryusb_plan.md)：阶段门禁、文本协议、启动时序和带职责注释的最终目录。
9. [USB 基线与验收](09_usb_validation.md)：实际构建结果、硬件待确认项与分阶段核验记录。
10. [Shell 与日志实施计划](10_shell_log_plan.md)：CherrySH/EasyLogger 阶段门禁、命令、接口归属及最终修改目录。
11. [Shell 与日志验收](11_shell_log_validation.md)：依赖版本、资源基线、阶段检查与三轮短时回归记录。
12. [FOC 控制设计](12_foc_design.md)：三种模式的基类、独立算法、BSP bind、中断回调链路、并发约束及最终目标修改目录。
13. [FOC 阶段 1 实施与验证](13_foc_stage1_validation.md)：实际绑定、test 命令、三轮短时回归、资源与实际修改目录；含用户授权后的四项限时 PWM 台架测试。
14. [FOC 阶段 2 开环电压](14_foc_stage2_open_voltage.md)：有界开环运行、保护、任务/IRQ 交接及三轮验证。

15. [FOC 阶段 3 校准与保存](15_foc_stage3_calibration.md)：校准流程、质量门槛、单页参数、三轮实板验证与实际文件清单。

16. [FOC 全部变动审查](16_foc_review.md)：诊断保护、生产/诊断耦合、冗余状态、停止事务及本轮注释/格式整理。

17. [FOC 审查问题修复](17_foc_fixes.md)：功能整改、资源变化、验证与最新累计实际修改目录。

18. [USART3 DMA 与 JustFloat](18_vofa_uart_dma.md)：COM9 数据链路、同周期遥测、独立任务、速率实测和实际修改目录。

`Docs` 根目录不再放专题文档，技术细节统一收在本目录。
`tests/` 仅保留本地，不入库。此记录不是实时状态源，继续开发前应重新检查源码和 Git 状态。

## 当前交付状态（2026-11-03）

FOC 与 USART3 DMA 遥测按四个功能阶段提交：硬件同步采样和算法、受保护开环、编码器校准与保存、JustFloat 遥测。四个提交快照均完成 Debug/Release 导出构建；分阶段主机检查通过。实板证据仍以 13～18 号记录所述范围为准，本次 Git 整理没有再次操作电机。

远端目标为 https://github.com/kaouduo/foc_gb4310_ll ，分支 main。下文开发过程中的“未提交”等表述为当时记录；提交标题与阶段结果见 [提交记录](01_commit_logs.md)。独立修改的 03_freertos_clion_debug.md 留在工作区，不纳入本次四笔提交。

## 开发阶段记录


阶段3功能版本已完成最终Debug→Release→Debug三轮实板检查：14极对、方向−1、全圈零偏4.468～4.469 rad，保存及重启加载通过；输出/采样关闭、COM11释放，未提交Git。阶段1～3的实现与验收历史分别保留在13～15记录。

最新一轮已实施审查问题整改：PWM 诊断复用运行保护、停止事务收敛、生产采零与 Debug 诊断分离、Release 移除高级自检、冗余实例/接口精简。构建及主机回归通过；2026-11-02 已完成整改后的 Debug→Release→Debug 三轮短实板回归，并补回新诊断文件实时 -O2 配置。当前保留 Debug，输出/采样关闭，串口释放。当前状态、证据及累计实际清单见 [17_foc_fixes.md](17_foc_fixes.md)。

实际编码器电压运行命令和电流闭环上电调参仍为后续阶段。


最新遥测已接入：USART3 PC10 TX、DMA1 Channel1、115200 8N1，最高100Hz/16通道，独立静态vofa任务1024B。2026-11-02 Debug→Release→Debug三轮COM9原始帧验证通过；高速配置在当前J-Link CDC UART接收不完整，先保留稳定配置。USB CDC Shell/日志独立；当前Debug、输出/采样关闭、两串口释放、未提交。见18记录。
