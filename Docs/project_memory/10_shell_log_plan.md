# CherrySH + EasyLogger 分阶段实施与验收计划

更新日期：2026-10-31（Asia/Taipei）。状态：S0–S4 已实施；Debug → Release → Debug 三轮短时实板检查完成，等待用户最终评估。实际结果见 [验收记录](11_shell_log_validation.md)。

## 范围与执行规则

在 foc_gb4310_ll 接入新版 CherrySH、EasyLogger，共用现有 CherryUSB CDC。复用 app_usb_task.c 管理收发与终端，不新增 Console 文件、不增加 Shell 或日志任务。接入 Shell 后删除旧简单解析器。

每阶段只实施当前阶段；交付文件差异、检查结果、资源增量、规范例外和待验证项后停止，等待用户审核。整改后重新交付，不自行进入下一阶段。未经单独授权不提交、不推送。

第三方完整源码不入库、不修改、不格式化；固定完整 commit、归档 SHA256 及直接编译输入/许可证校验清单，不自动追随 master。自建代码遵循现有六层架构、UTF-8、中文 Doxygen、作者 kaouduo 和根 clang-format。

用户追加要求：本次同步把统一工具链升级至 C23/C++23。S0 核对编译参数、两种固件和语法检查；发现的项目兼容问题在当前阶段修复并记录，不修改第三方。此项使工具链、业务状态初始化及当前架构/接口文档提前归入 S0。

不进行长时间压力测试，最终只执行三轮完整短时回归，由用户评估。主机检查、构建与实板结果分开记录，未验证不写成通过。

## 已确定功能

| 项目 | 约定 |
|---|---|
| 用户与提示符 | 默认 root，空密码，无登录问答；提示符 `root> `，root 绿色加粗 |
| Debug/Release Shell | 均免密码，保留系统命令 |
| 编辑 | 字符回显、退格、删除、光标移动、历史、Tab 补全 |
| 日志 | ERROR/WARN/INFO/DEBUG，时间、模块标签、颜色、异步 CDC 发送 |
| Debug | 编译保留 DEBUG，默认 DEBUG；注册 log 命令 |
| Release | 编译移除 DEBUG，固定 INFO；不注册 log 命令 |
| 暂不启用 | 按模块过滤、多后端独立配置、ISR 日志、Flash 日志、完整启动历史 |

### 系统命令

| 命令 | 输出与动作 |
|---|---|
| help | 使用上游帮助，列出本构建实际注册命令及用法 |
| reboot | `OK reboot\r\n`，发送完成或最多等待 200 ms 后普通复位 |
| clear | 清历史并清屏，恢复提示符；不能找回清理前历史或 clear 自身 |
| dfu | `OK dfu\r\n`，发送完成或最多等待 200 ms 后进入现有 ROM USB DFU |
| system uid | `UID=<24位大写十六进制>\r\n`，与 USB 序列号一致 |
| system time | `TIME_MS=<十进制毫秒数>\r\n`，来自 BSP 运行时间接口 |
| test enc | 单次读取 MT6826S，输出原始计数、角度及状态；失败仅报告错误，不启动周期采样 |

命令区分大小写；缺少/多余参数、未知子命令报告错误及用法，不执行动作。支持 CR、LF、CRLF，CRLF 不重复执行。未知命令不再按旧协议原样回显，不保留顶层 uid 别名。

time 为调度器启动后的 32 位毫秒数，不是 RTC，沿用现有回绕契约。reboot/dfu 接受后停止处理后续输入及新日志发送，优先完成确认响应；即使随后断线，也执行已接受动作。

### Debug 专用日志命令

| 命令 | 行为 |
|---|---|
| log level | 查询等级 |
| log level off/error/warn/info/debug | 设置全局等级；off 停止后续日志，不撤回已排队记录 |
| log color | 查询颜色 |
| log color on/off | 设置颜色 |
| log stats | 查询累计丢弃和截断条数 |

clear 不清日志统计。Release 从源码编译和命令表移除 log 命令，帮助/补全均不出现。

### 日志输出

```text
[1.264] I usb: CDC initialized
[881.616] W encoder: Read retry
```

产生记录时取时间，以整数拆分秒和三位毫秒，不使用浮点。ERROR 红、WARN 黄、INFO 绿、DEBUG 青，每条彩色记录恢复默认颜色。模块标签仅显示，不做模块过滤。

无新增 printf 全局重定向。命令结果不套日志前缀。普通日志只在任务上下文调用，不接管 FreeRTOS 断言或 HardFault；不开放 EasyLogger 其他等级为业务日志接口。

单条发送记录上限 192 B（含颜色、前缀、CRLF），保留尾部空间，超长标记截断并计数。调用任务格式化后整条复制入队，不等待 USB；队列满丢弃新记录并计数。项目入口添加 GCC 格式检查属性。

## 职责与接口归属

| 位置 | 职责与边界 |
|---|---|
| main | 启动编排、创建任务、绑定输出回调并初始化日志/资源 |
| application/task/app_task.h | 只集中声明任务入口，不承载 USB 输出接口 |
| application/task/app_usb_task.c/.h | 唯一 CDC 读写者；输出初始化、日志提交回调、队列、分包/通知、会话与动作交接 |
| application/shell/app_shell.c/.h | Shell 对象/静态缓冲、会话、读写回调绑定、输入与编辑显示恢复 |
| application/shell/commands | 参数解析、命令注册与业务/BSP 调用，不直接访问端点/LL |
| application/log/app_log.c/.h | 日志入口、等级/颜色、统计与 EasyLogger 初始化 |
| porting/cherrysh | 库配置，不放业务命令或 USB 硬件实现 |
| porting/easylogger | 静态锁、时间与输出钩子；通过绑定回调输出，不反向包含 application 头 |
| bsp/usb、bsp/platform | 复用已验证的 CDC、时间、UID、复位及 DFU |

应用可使用中间件；硬件访问经过 BSP，main 保留启动编排权限。新公共头独立提供 C/C++ 链接保护；日志调用者不需要包含 FreeRTOS/LL。依赖绑定在首次使用前完成，不添加重复初始化或多余上下文检查。

计划启动顺序：创建 USB 任务（先等 START）→ 初始化输出资源及日志锁、绑定输出、启用日志 → 初始化资源并记录结果 → 启动 CDC → 成功后通知 USB 任务 → 已配置后建立 root Shell 会话。初始化失败保持现有故障/LED 行为；无主机不阻塞启动。

## 阶段 0：基线、文档与固定依赖

- 合并原阶段 0、1。检查 Git 状态，保留用户已有修改。
- 构建 Debug/Release，记录 Flash/RAM、任务栈与功能基线。
- 升级 C23/C++23（REQUIRED、关闭扩展）；C23 已移除的 ATOMIC_VAR_INIT 改为原子对象直接静态初始化，不改变初值与运行行为。独立验证 C23/C++23 语法和公共头。
- 锁定官方最新提交：CherrySH `c8c1c0964bc0033f24efb255cd607b35e3a19490`；EasyLogger `806328e131836662fa83dd43364a53f699fd76ac`。归档与校验值见验收记录及 CMake。
- 下载完整源码，建立项目配置、归档目标和校验清单。CherrySH 只编译核心/readline；help 在 S2 直接编入固件，避免静态库命令未被提取。上游 clear/login/示例不编译。
- CherrySH 单实例、非阻塞、静态行缓冲、历史/补全，关闭多线程执行及 XTerm 探测。EasyLogger 核心关闭库内异步、缓冲输出及 pthread，不编译插件。
- 链接脚本增加对齐、FSymTab/VSymTab 边界和 KEEP，不改变存储布局。
- 本阶段仅归档两个库，不链接固件、不初始化，不要求运行时钩子已实现。空命令段不应增加固件 RAM/Flash。
- 门禁：首次获取、离线构建、缺文件/哈希错误检测、编译清单与 ELF 符号检查通过，Debug/Release 成功。原运行行为不变属于代码路径结论，不冒充实板回归。
- 交付后停止。

## 阶段 1：复用 USB 任务建立输出管理

- app_usb_task.c 内使用初始 1024 B 静态日志记录存储；不增加 Console 文件或第二个发送队列。对外接口放新增 app_usb_task.h，不放 app_task.h。
- S1 接口为 App_USBOutput_Init/Stop/LogWrite/DroppedLogs/Write。Init 由启动任务传入已创建的 TaskHandle_t，任务专用头允许引用 FreeRTOS；未来普通日志调用者只需 app_log.h。初始化失败先 Stop 再删除任务，禁止生产者通知已删除句柄。
- Shell 输出由同一 USB 任务通过助手分包；回调返回前消费或复制，不能保存临时指针。背压最多等待 100 ms，等待时推进 TX 并阻塞让出 CPU，不等待自身消费而死锁。
- 部分写入/超时标记显示失效，在本轮处理返回后重建会话；不伪报完整输出。
- 日志完整入队，满则整条丢弃并计数，提交后通知任务；日志显示及清行/重绘期间不递归解析输入或消费其他日志。
- 每轮最多发送一条日志后检查输入，避免交互饿死；64 B 提交、TX_DONE、ZLP 继续使用现有 BSP 契约。
- 临界区只保护队列/提交，不格式化、不等待。SessionId 改变时清理旧输入/输出，防止跨会话重放；无连接不阻塞启动。
- 暂留旧解析器验证传输助手。门禁覆盖 1/63/64/65 B、长输出、BUSY、拥塞、通知、超时和会话恢复，交付 RAM 增量后停止。
- S1 实际：2 B 长度头+变长正文的 1024 B 环形存储，单条 1–192 B，发送完成前不释放记录，无额外整条暂存区。发送失败/会话取消/关闭时作废的日志纳入丢弃统计；首个有效连接前的暂存允许保留。
- S1 保持当前资源初始化顺序，只在已创建 USB 等待任务之后、CDC 初始化之前初始化输出；S3 启用资源启动日志时再将任务与日志初始化提前到资源初始化之前。

## 阶段 2：CherrySH 与系统命令、清理旧入口

- 初始输入缓冲 128 B、提示符 64 B、历史 256 B、最大参数 8、补全候选 16；有效输入长度按锁定版本核对。USB 栈先设 2048 B，水位复核。
- 配置 PATH=/bin:/sbin，root、空密码、root> 提示符。接收沿用当前 CDC 背压，不迁移参考工程的 RX 丢字节环或 HAL/CMSIS 适配。
- 实现系统命令，命令通过 BSP；clear 请求在 REPL 返回后重建历史/编辑状态并清屏，避免破坏执行中的库对象。
- 接受 reboot/dfu 后由 USB 任务完成响应及动作交接。重连清半条输入与历史并显示新提示符。
- 删除 app_usb_console.c/.h、旧 CR 超时/行回显/字符串匹配、CMake 源项和失效包含路径；平台动作类型迁至 app_shell.h。
- 门禁覆盖编辑/补全/历史、清理、CR/LF/CRLF、分包/多行、错误参数、超长恢复、重连及动作时序，交付后停止。

## 阶段 3：EasyLogger 与输入行协作

- 实现 LOG_E/W/I/D 和等级预检查，避免被过滤日志仍计算昂贵参数。静态 FreeRTOS 锁管理库共享状态；锁内不等待 USB。
- 配置秒格式时间、等级、标签、颜色、长度与统计。实现 Debug 专用 log 命令。
- 当前上游原生前缀与目标格式不同；由项目适配完成目标格式与截断统计，不修改第三方源码，不声称仅配置即可得到精确目标格式。
- 复用 USB 任务队列。正在编辑时清显示→发日志→重绘输入和光标；命令执行/结果输出期间暂不消费日志，不插提示符。控制台消费路径不再记录日志，防止递归。
- 在资源初始化结果和整机状态变化处接入日志，不周期刷屏；首个有效会话前仅有界暂存，后续会话重置不重放旧队列。
- 门禁覆盖等级/OFF、Debug/Release、颜色、截断、统计、多任务提交、编辑中插日志、命令输出及断线恢复，交付后停止。

## 阶段 4：三轮短时回归与最终核对

第一轮 Debug、第二轮 Release、第三轮 Debug。每轮完整执行：

1. root 免密码、提示符、帮助、补全。
2. system uid/time、UID 与 USB 序列号一致。
3. 退格/删除/光标/历史、CR/LF/CRLF、错误命令和超长恢复。
4. clear 清屏及历史，不留下 clear 自身。
5. 日志颜色与输入恢复；Debug 验证 log，Release 确认没有该命令。
6. 短时队列拥塞，检查整条丢弃、统计和恢复。
7. 关闭再打开串口，继续交互。
8. reboot 一次，重新枚举后继续交互。
9. dfu 一次，确认 ROM DFU；保护参数页、恢复同轮应用并检查 Shell，不整片擦除。

使用 CLion MCP 与 clion-debugger 检查必要的初始化顺序、变量、队列、调用栈、任务栈；正常传输测量不使用停机断点。原生 logpoint 无输出不能作为未执行证据。发生 Cortex-M Fault 时使用 clion-embedded-hardfault 读故障寄存器、异常帧及故障指令。

检查 startup/USB 任务最低栈余量目标 256 B；不足调整并复核。失败保留记录，整改补测；三轮不替代长期稳定性或未来 FOC 实时性验证。硬件不可用时明确未执行，不用主机检查替代实板。

最终逐项核对目录、依赖版本、资源增量、三轮结果及未验证项，交付后停止等待用户评估。

## 最终修改目录（累计至 S4）与阶段归属

只列相关文件。`[+]` 新增、`[M]` 修改、`[-]` 删除、`[下载]` 不入库；S0–S4 为首次实施及后续完善阶段。

```text
foc_gb4310_ll/
├─ [M/S0–S3] CMakeLists.txt                 # 显式源码、库、Debug 命令；S2 删除旧解析器
├─ [M/S0] .gitignore                        # 忽略下载目录
├─ [M/S0] STM32G431CBUX_FLASH.ld            # 命令段 KEEP/对齐/边界，存储布局不变
├─ cmake/
│  ├─ [+/S0] cherrysh.cmake                 # 固定提交、下载、校验、核心归档
│  ├─ [+/S0] cherrysh.sha256                # 构建输入/help/许可证校验
│  ├─ [+/S0] easylogger.cmake               # 固定提交、下载、校验、核心归档
│  ├─ [+/S0] easylogger.sha256              # 构建输入与许可证校验
│  └─ [M/S0] gcc-arm-none-eabi.cmake        # 用户追加 C23/C++23，REQUIRED、关闭扩展
├─ Docs/project_memory/
│  ├─ [M/S0,S4] README.md                   # 索引
│  ├─ [M/S4] 02_session_context.md          # 最终功能、实测与边界
│  ├─ [M/S0,S4] 04_architecture.md          # C23/C++23；S3 日志与会话职责
│  ├─ [M/S4] 05_platform_rtos.md            # 日志初始化、锁、USB 栈及三轮水位
│  ├─ [M/S0] 07_interface_rules.md          # C/C++ 标准约定同步
│  ├─ [+/S0–S4] 10_shell_log_plan.md        # 计划、阶段、最终目录
│  └─ [+/S0–S4] 11_shell_log_validation.md  # 基线、检查、三轮、目录对照
└─ project/
   ├─ application/
   │  ├─ [M/S1–S4] main.c                  # 调度前日志→启动任务→资源→CDC
   │  ├─ state/
   │  │  └─ [M/S0] app_device_state.c       # C23 原子对象直接静态初始化
   │  ├─ task/
   │  │  ├─ [M/S2] app_task.h               # USB 栈注释同步到 2048 B
   │  │  ├─ [M/S3] app_led_task.c           # 仅状态/故障变化时记录日志
   │  │  ├─ [+/S1,S3] app_usb_task.h           # 输出初始化/关闭、记录提交、统计及有限发送
   │  │  └─ [M/S1–S3] app_usb_task.c        # 队列、CDC、Shell 调度、日志重绘、动作
   │  ├─ shell/
   │  │  ├─ [+/S2–S3] app_shell.c          # root 会话/缓冲/输入/编辑恢复
   │  │  ├─ [+/S2–S3] app_shell.h          # Shell 接口、IO 回调、动作类型
   │  │  └─ commands/
   │  │     ├─ [+/S2] cmd_console.c        # clear；help 编译上游实现
   │  │     ├─ [+/S2] cmd_system.c         # reboot/dfu/system uid/time
   │  │     ├─ [+/S2,S3] cmd_test.c         # test enc；Debug 记录单次读取结果
   │  │     └─ [+/S3] cmd_log.c            # 仅 Debug 的 log 命令
   │  ├─ log/
   │  │  ├─ [+/S3] app_log.c               # 入口、格式、过滤、配置及统计
   │  │  └─ [+/S3] app_log.h               # LOG_E/W/I/D 与公共接口
   │  └─ usb/
   │     ├─ [-/S2] app_usb_console.c       # 删除简单解析器
   │     └─ [-/S2] app_usb_console.h       # 动作类型迁至 Shell
   ├─ bsp/usb/
   │  ├─ [M/S3修正] bsp_usb_cdc.c           # 透传终端就绪查询，不改变非阻塞传输
   │  └─ [M/S3修正] bsp_usb_cdc.h           # 终端就绪查询契约
   ├─ common/
   │  └─ [M/S1] usb_cdc_types.h             # USB 事件契约注释
   ├─ porting/
   │  ├─ freertos/
   │  │  └─ [M/S3] FreeRTOSConfig.h         # 允许查询调度器状态，早期日志不等待互斥锁
   │  ├─ cherryusb/
   │  │  ├─ [M/S3修正] usb_cdc.c            # DTR/实际 OUT 数据标记终端就绪
   │  │  └─ [M/S3修正] usb_cdc.h            # 底层终端状态查询
   │  ├─ cherrysh/
   │  │  └─ [+/S0,S2] csh_config.h         # 单实例、非阻塞、静态缓冲等配置
   │  └─ easylogger/
   │     ├─ [+/S0,S3] elog_cfg.h           # 编译等级、格式、颜色、容量
   │     ├─ [+/S3] elog_port.c             # 静态锁、时间、绑定输出
   │     └─ [+/S3] elog_port.h             # 平台初始化与回调契约
   └─ middleware/third_party/
      ├─ [M/S0,S4] README.md               # 版本、哈希、获取与构建方式
      ├─ [下载/S0] CherrySH/               # 完整上游，不改；无默认 clear/login
      └─ [下载/S0] EasyLogger/             # 完整上游，只选核心
```

app_task.h 继续只声明任务入口，不增加输出管理接口；不新增 app_console.c/.h 或 app_shell_task.c。为修复首连启动日志丢失，S3 补充 BSP/USB CDC 终端状态查询和 DTR 回调；原非阻塞读写、端点封装和平台 UID/时间/ROM DFU 保持原契约。tests/ 本地忽略，脚本和结果位置记录于验收文档。

## 参考工程对照与遗漏检查

参考 foc_drive_software 的 app_shell、app_shell_task、cherrysh_usb、usb_cdc_log、cmd_system 及链接脚本。

| 检查项 | 本次处理 |
|---|---|
| 静态库命令被回收/未提取 | 链接 KEEP；help 在 S2 直接编入固件；核对命令表 |
| 路径搜索 | 导出路径与 PATH=/bin:/sbin 对齐 |
| 默认 clear 只清屏 | 项目命令请求清屏并清历史，返回后重建状态 |
| 旧 user/qdrive 会话 | 改 root、空密码、root> |
| 100 ms 部分写入 | 有限等待、失败标记与显示恢复，不能假报成功 |
| 日志/输入交错 | 唯一消费者串行输出及恢复输入行 |
| 同一任务背压死锁 | Shell 写回调主动推进 TX，不能等自身队列消费 |
| 仅判断 configured | 复用当前 SessionId，旧响应不跨会话 |
| HAL/CMSIS 轮询、RX 字节丢弃 | 不移植，保留现有 LL/FreeRTOS 通知及 RX 背压 |
| 电机运行时重启保护 | 当前无控制业务；未来启用 FOC 前补停机交接，不引入不存在的接口 |
| printf 绕开管理 | 不新增全局重定向，使用统一命令/日志入口 |

最终每个新增/修改/删除文件必须与目录对应；目录调整先说明理由并同步文档。无双解析器、重复命令、多个 CDC 写入者、额外日志任务或未经确认的扩展。


## 2026-10-31 用户补充：提示符、补全与早期日志

- S2 提示符调整为绿色加粗 `root`，`> ` 默认颜色；使用 readline 的样式字段，保持可见宽度计算正确。
- 应用层接入补全回调，保留上游命令名/变量补全，增加 `test e<Tab>`、`system u<Tab>`、`system t<Tab>`。不修改第三方源码，不新增生产文件。
- S3 必须在启动任务创建前支持记录初始化日志；在基础时钟、时间源可用后初始化日志入口，早期路径不得依赖已创建任务、任务通知或调度器锁。
- USB CDC 初始化和主机枚举前不能实际向终端发送。早期日志应进入有界缓冲，CDC 就绪后顺序输出，并保留记录时的时间戳；缓冲满计丢弃，禁止阻塞启动等待主机。
- 优先复用既有输出环形存储，绑定 USB 任务时不得重置早期记录，避免新增大缓冲；S2 静态 RAM 使用 30496 B；S3 Debug 为 30960 B，剩余链接空间 1808 B，不等于可用堆/栈。
- USB 永远未就绪时无法通过 CDC 显示启动失败；本需求不引入额外 UART 后端。
- `exit` 暂不添加；EasyLogger 尚未接入，以上早期日志已在 S3 实现。


## S3 落地约束与验收入口

- main 在时钟/DWT 就绪后绑定输出和时间回调、初始化 EasyLogger，随后记录基础初始化和 LL IO 初始化，再创建启动任务。调度前时间源为 0，因此显示 `[0.000]`；时间仍保持调度器运行时间定义。
- 绑定 USB 任务之前只有 main/启动任务这一条日志生产路径，不允许 ISR 或其他任务生产，不使用调度前的队列任务临界区；绑定后使用可嵌套的 FreeRTOS 短临界区。绑定操作不清队列或统计。首个成功发送提示符的终端会话前有界暂存，后续会话变化丢弃旧记录。
- 移植代码只依赖 EasyLogger、FreeRTOS、CMSIS，不反向引用应用/BSP。main 注入静态生命周期的锁/解锁/ISR 检测能力到 app_log；业务门面不直接依赖 porting。输出/时间回调同样由 main 绑定。
- 静态互斥保护库共享格式缓冲、格式/颜色配置和统计；EasyLogger 内部重复加锁关闭，业务仅通过 LOG_E/W/I/D 门面使用库，持锁不等待 CDC。未接入 ISR/异常日志。
- Debug 默认 DEBUG，Release 默认 INFO；Debug 可运行时降低等级或关闭；Release 编译去除 LOG_D 参数和调用，移除 cmd_log 源码和命令记录。
- `log level`、`log color` 查询；带参数设置。`log stats` 包含 emitted（交给库输出的记录，可能入队失败）、filtered（运行时预检查/复查拒绝次数）、truncated（消息正文截断次数）、dropped（队列拒绝或作废记录）及当前等级/颜色。Release 编译消除的 DEBUG 不计 filtered。标签最多显示 16 字节，正文最多 143 字节，超长以 `...` 标记；最终完整记录不超过 192 字节。
- 日志出队仅在命令函数返回后进行；清除当前显示行，发送日志，恢复提示符、原始编辑内容和光标，不清历史或分包 ESC 状态。显示失败停止本次绘制，由既有会话恢复路径清理。
- 当前 readline 逻辑列宽固定 160，保证 125 字节行缓冲不被上游刷新截断；物理终端较窄时长行显示可能折行。尚未实现窗口尺寸协商。
- 用户已确认 Shell 几个命令实板测试；本阶段日志未烧录验证，下一阶段仅进行既定三轮短时回归。S4 再更新 02_session_context.md、05_platform_rtos.md，当前目录未把它们列为已修改。


## S3 实板反馈修正：启动日志全部丢弃（2026-10-31）

用户实测首个 `log stats` 为 emitted=6、filtered=1、dropped=6，后续手动开启 DEBUG 后日志正常。初版错误地把 USB 配置就绪直接视为会话建立，首次提示符发送超时会触发整队列丢弃。

现增加独立 IsTerminalOpen 查询：DTR 打开或实际 OUT 数据标记终端就绪，复位/重新配置时清除。DTR 更新只通知任务，不发送、不等待，不改变 BSP Read/Write 的既有契约。不切换 DTR 的终端可发送一个字符或 Enter 唤醒。

任务等待终端就绪后再建立 Shell；首次提示符全部发送成功后才设置 sessionEstablished。在此之前的显示失败、总线代次改变保留启动队列；已建立会话的旧日志仍按原规则作废。终端关闭再打开时重建 Shell，不重放旧运行日志。成功发送只表示主机 USB 接受，不保证终端应用已显示。

按用户最新要求，Debug 默认 DEBUG，Release 仍 INFO。本修正没有创建新任务/大缓冲；新增修改目录为 bsp/usb 两个文件和 porting/cherryusb 两个文件，已同步到最终目录。累计 40 个交付文件（排除用户自有调试文档修改）。


## S3 日志中文帮助与参数补全（2026-10-31）

用户已提供七条启动日志、运行 DEBUG 输出、颜色关闭、UID/时间和 emitted=11/filtered=0/truncated=0/dropped=0，确认首连启动缓存修正有效。最终三轮实板回归仍未执行。

本轮仅修改 app_shell.c 与 cmd_log.c 及本计划/验收记录，不增加文件。`help` 中 log 简介改为中文，`help log` 为 level/color/stats 添加中文说明与统计字段解释。命令关键字仍使用英文，中文仅用于输出。

Debug 支持二级命令 level/color/stats 以及三级等级 off/error/warn/info/debug、颜色 on/off 的 Tab 补全。空参数 Tab 可列出候选；存在共同前缀时先补共同前缀，再按 Tab 列候选，例如 `log color <Tab>` 先补 o，下一次列出 on/off。参数补全保留命令原有参数校验，Release 不包含 log 子命令候选。保留 system/test 补全，不修改第三方库。


## S4 最终交付与目录核对

Debug / Release / Debug 三轮实板串口主检查分别 31 / 24 / 31 项，均完成串口重开、reboot、ROM DFU，并通过 USB 恢复同轮 ELF、校验和启动。预置满队列分别拒绝 7 / 6 / 7 条实际启动日志，六条旧记录保持完整，排空后恢复。未执行旧 600 秒压力脚本。

三任务剩余栈均大于 256 B，参数页全程一致。CLion 启动未形成会话，回退到 GDB/J-Link。ROM GO 后调试连接异常通过额外普通软件复位恢复；首轮 GDB 目标函数注入未返回，已中止并复位，正式有界拥塞改用预置队列，不注入函数调用。

S4 仅同步 02/04/05/README/10/11 文档，生产代码未再修改。累计 42 个交付新增/修改/删除文件均列入最终目录；用户原有 03_freertos_clion_debug.md 修改不属于本次交付，未改动。本地测试忽略，无 Git 提交或推送。

最终目标运行正常 Debug，默认 DEBUG、颜色开、正常启动 dropped=0；COM11 释放、测试 J-Link 服务器关闭，未改用户 IDE 断点。等待用户最终评估。
