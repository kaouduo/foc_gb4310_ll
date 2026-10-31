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

- 用户确定优先采用 CherryUSB，并希望了解其内部实现；CherryUSB CDC 已接入，上层现为 CherrySH/EasyLogger。
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

- FOC 算法、TIM1 完整控制初始化、PWM 使能/关闭、死区与硬件保护、ADC 同步采样、20 kHz 控制中断均未实现；Shell 与日志已接入。
- 其他早期 porting IO 模块仍直接使用 `bsp_board_config`，后续实现时要按 BSP 传配置的方向收齐，不复制旧 HAL 分层问题。
- STANDBY 只说明已接入资源初始化成功，不代表所有电机控制与保护资源已就绪。
- 接电机业务前需明确：状态转换的管理者、故障后的重新初始化与恢复条件、异常路径的功率输出关闭入口。
- MT6826S 已由 test enc 按需单次读取；接入 20 kHz 中断前仍需协调访问归属并实测耗时。

## 验证状态

- 阶段 5 `cmake --build --preset REV6-Debug` 与 `REV6-Release` 均通过（Debug FLASH 54764 B / 42.44%、Release 26824 B / 20.79%，RAM 27904 B / 85.16%）。
- 主机测试覆盖启动顺序、状态与灯效、LED 基类分派、MT6826S 协议与方向位、Flash 边界、DWT/tick 回绕；`tests/` 不入库，不能当作仓库自带测试能力。
- USB 文本/复位/ROM DFU 和阶段 5 自动联合验收已实板执行，范围与限制见 09_usb_validation.md。连续传输中发现的待取包通知问题已在 app_usb_task 修正并经主机回归与实板长测通过。


## Shell / 日志最终状态（2026-10-31）

CherrySH root 免密码，支持 help/clear/reboot/dfu/system uid/time/test enc、编辑、历史和参数补全；旧简单解析器已删除。EasyLogger 在创建启动任务前初始化，共用现有 USB 任务和 1 KB 队列，无新增任务。DTR 或实际 OUT 数据后开始显示，首次提示符成功前保留启动缓存。时间为调度器运行秒与三位毫秒，调度前 [0.000]；system time 仍为整数毫秒。

Debug 默认 DEBUG，含中文 log 帮助与二三级补全；Release 默认 INFO，不编译 log 命令及 LOG_D。未启用 ISR 日志、模块过滤、多后端和 Flash 日志。

S4 已完成 Debug → Release → Debug 三轮短时实板检查；详见 10_shell_log_plan.md / 11_shell_log_validation.md。最后 2 KB 参数页哈希一致。Debug Flash 82596 B、RAM 30960 B；Release 42156 B、30912 B。实测最低剩余栈 startup 1260 B、USB 816 B、LED 1848 B。最终正常 Debug、默认 DEBUG/颜色开、dropped=0，串口释放，测试服务器关闭。未提交或推送，等待用户最终评估。

当前追加修正：startup 已改为 xTaskCreate 动态创建、自删除后由空闲任务回收，移除静态栈/控制块；USB/LED 保持静态。Debug RAM 28808 B，Release 28760 B，堆仍 16 KB。主机启动路径与 Debug/Release 构建通过，尚未烧录此修正版；先前三轮实板属于修正前版本。


## 本轮 Git 提交整理（2026-10-31）

按用户要求整理为六个逻辑提交：固定依赖、C23/C++23、USB 输出管理、CherrySH 系统命令、EasyLogger 启动日志与终端控制、startup 动态创建。首连修正与中文帮助/补全并入 Shell/日志功能，不单独提交验收记录，文档随实现提交。每个暂存阶段均在独立导出目录完成 Debug/Release 构建；startup 动态版本的主机检查此前通过，尚未上板。仅本地提交，未推送；用户原有 03_freertos_clion_debug.md 修改留在工作区。
