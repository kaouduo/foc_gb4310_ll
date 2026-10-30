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

`Docs` 根目录不再放专题文档，技术细节统一收在本目录。
`tests/` 仅保留本地，不入库。此记录不是实时状态源，继续开发前应重新检查源码和 Git 状态。
