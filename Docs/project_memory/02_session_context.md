# 对话工作与工程约束

更新日期：2026-10-31。以当日读取的源码为当前快照；后续文件与此记录冲突时，先检查最新源码和用户要求。

## 工程位置与参考资料

当前开发工程：`E:/my-documents/research/stm32/my_gb4310_drive/foc_gb4310_ll`。
不要误将会话环境中的 `foc_drive_software` 当作当前待修改工程。
参考工程：同级 `my_gb4310_drive` 与 `foc_drive_software`；前者存在多种编码器历史实现，当前板明确使用 MT6826S，不使用 AS5047P。
架构及注释依据：`D:/Documents/obsidian/Engineer/project/README.md`。用户在对话中确定的设计优先于旧规范。

## 用户确定的长期规则

- 未经用户审核同意，不执行 Git 提交或推送。
- 提交日志 `type(module): 中文描述`；MR `[类型] 中文描述`，正文仅修改原因、修改内容、验证。
- 不把对话中的临时小问题写成提交日志。`tests/` 保留本地并忽略，不入仓库。
- 自建文本 UTF-8，代码按根 `.clang-format` 格式化；中文 Doxygen 注释，作者 `kaouduo`；第三方代码不改、不格式化。
- 不增加固定配置前提下多余的空指针、重复初始化、上下文防御检查（详见 [接口与编码约定](07_interface_rules.md)）。
- 不为简单器件强行增加继承或空层：单实例编码器用 BSP 私有对象 + 薄接口，LED 因多实例与 GPIO/PWM 双实现才用基类。
- 文档只保留在 `Docs/project_memory/`；`Docs` 根目录不再放专题文档，避免与记忆重复。

## USB 选型与初始化代码风格

- 用户确定优先采用 CherryUSB，并希望了解其内部实现；后续 USB 移植按此选型推进，当前尚未接入协议栈。
- 时钟及外设初始化代码参考 CubeMX 的 LL 生成风格，便于与生成代码对照阅读；保留项目分层、中文注释及真实硬件等待的超时返回，不因此引入整套 HAL 或覆盖现有工程。

## 记录索引

| 主题 | 文件 |
|---|---|
| 分层架构、硬件引脚、启动顺序、构建参数 | [04_architecture.md](04_architecture.md) |
| BSP_Platform、DWT、时间、Flash 服务、FreeRTOS 配置与任务 | [05_platform_rtos.md](05_platform_rtos.md) |
| 设备状态机、LED 灯效、MT6826S | [06_devices_business.md](06_devices_business.md) |
| 防御检查边界、C/C++ 混合、编码与提交规范 | [07_interface_rules.md](07_interface_rules.md) |
| CLion FreeRTOS 任务视图 | [03_freertos_clion_debug.md](03_freertos_clion_debug.md) |
| 提交记录 | [01_commit_logs.md](01_commit_logs.md) |

## 未完成与后续边界

- 用户明确 main 启动编排允许直接调用 porting，其他 app task/业务模块通过 BSP。当前 main 直接调用 System_Clock_Init、DWT_Init，已删除 BSP_Platform_Init。
- CherryUSB 阶段 5 自动联合验收完成：29 项文本边界、串口重开 20 次、持续插线 reboot 20 次、600.28 秒/3,534,960 B 逐字节回显、DFU 下载/应用读回及空参数页一致性通过；USB 栈最低剩余 736 B，LED 栈 1800 B。人工拔插由用户取消，等待最终审核；详见 09_usb_validation.md。
- CLion 的 J-Link 调试配置启用持久会话（Persistent session）；用户确认该配置下载更快，CDC 可正常识别。配置说明见 03_freertos_clion_debug.md。

- FOC 算法、TIM1 完整控制初始化、PWM 使能/关闭、死区与硬件保护、ADC 同步采样、20 kHz 控制中断、shell 均未实现。
- 其他早期 porting IO 模块仍直接使用 `bsp_board_config`，后续实现时要按 BSP 传配置的方向收齐，不复制旧 HAL 分层问题。
- STANDBY 只说明已接入资源初始化成功，不代表所有电机控制与保护资源已就绪。
- 接电机业务前需明确：状态转换的管理者、故障后的重新初始化与恢复条件、异常路径的功率输出关闭入口。
- MT6826S 读取接口当前无调用方；接入 20 kHz 中断与 shell 前需协调访问归属并实测耗时。

## 验证状态

- 阶段 5 `cmake --build --preset REV6-Debug` 与 `REV6-Release` 均通过（Debug FLASH 54764 B / 42.44%、Release 26824 B / 20.79%，RAM 27904 B / 85.16%）。
- 主机测试覆盖启动顺序、状态与灯效、LED 基类分派、MT6826S 协议与方向位、Flash 边界、DWT/tick 回绕；`tests/` 不入库，不能当作仓库自带测试能力。
- USB 文本/复位/ROM DFU 和阶段 5 自动联合验收已实板执行，范围与限制见 09_usb_validation.md。连续传输中发现的待取包通知问题已在 app_usb_task 修正并经主机回归与实板长测通过。
