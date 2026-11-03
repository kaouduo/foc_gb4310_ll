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

`Docs` 根目录不再放专题文档，技术细节统一收在本目录。
`tests/` 仅保留本地，不入库。此记录不是实时状态源，继续开发前应重新检查源码和 Git 状态。

当前进度：S2 Shell 命令已由用户测试；S3 EasyLogger 与早期日志已实现并完成主机/构建检查，等待实板日志验收。最终三轮短时回归尚未执行。

S3 首连修正已完成主机/构建检查：Debug 默认 DEBUG，使用 DTR/OUT 数据等待终端就绪，首次提示符成功前保留启动缓存；等待用户实板重测。

最终状态：Shell/日志 S4 三轮短时实板检查完成，正常 Debug、串口释放、测试服务器关闭，等待用户最终评估；以 11_shell_log_validation.md 最后 S4 记录为准。

最新追加：startup 动态创建修正已通过主机/两构建检查，尚未烧录；此前三轮实板结果属于静态 startup 版本。


## FOC 阶段 1 提交状态

独立 FOC 算法、BSP 硬件绑定和关闭输出的采样诊断；尚不开放带电运行命令。

本提交为按功能依赖整理的源码快照。实板历史记录按各阶段文档阅读；提交前另行构建 Debug/Release，不将历史测试冒充中间快照再次实板验收。
