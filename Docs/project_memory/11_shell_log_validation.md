# Shell 与日志基线和验收记录

日期：2026-10-31（Asia/Taipei）。本记录仅把实际执行的检查标记为通过。阶段要求与最终目录见 [计划](10_shell_log_plan.md)。

## 阶段状态

| 阶段 | 状态 |
|---|---|
| S0 基线、文档、固定依赖 | 实施及检查完成；用户已授权进入 S1 |
| S1 USB 任务输出管理 | 完成，用户已授权进入后续阶段 |
| S2 CherrySH 与命令 | 完成；用户确认 Shell 命令实板测试 |
| S3 EasyLogger 与显示协作 | 完成；用户已提供启动日志、运行日志与 dropped=0 |
| S4 三轮短时回归 | Debug / Release / Debug 三轮完成，等待用户最终评估 |

S3 未烧录、复位或操作实板，未运行 CLion 调试会话。Shell 已接入、旧解析器已删除；用户确认此前几个 Shell 命令实板测试。EasyLogger 已启用，但新日志功能尚未实板验收。下文 S0/S1 的记录为历史快照。

用户在 S0 实施期间追加并确认：C、C++ 均升级到 23。已修改统一工具链，并修正项目原子初始化的 C23 兼容问题，未改第三方源码。

## 依赖固定版本

查询官方 master 后固定具体提交，不使用浮动分支下载。

| 项目 | CherrySH | EasyLogger |
|---|---|---|
| 仓库 | cherry-embedded/CherrySH | armink/EasyLogger |
| commit | `c8c1c0964bc0033f24efb255cd607b35e3a19490` | `806328e131836662fa83dd43364a53f699fd76ac` |
| 提交日期 | 2026-10-17 | 2026-08-13 |
| 许可证 | Apache-2.0 | MIT |
| 上游标识 | 按 commit 管理 | 头文件版本 2.2.99，仍按 commit 固定 |
| 目标 | cherrysh 静态库 | easylogger 静态库 |
| 本阶段源码 | chry_shell.c、cherryrl/chry_readline.c | easylogger/src/elog.c、elog_utils.c |
| 上游输入校验条数 | 9（含未来使用的 help） | 4 |

归档 URL：

- `https://codeload.github.com/cherry-embedded/CherrySH/zip/c8c1c0964bc0033f24efb255cd607b35e3a19490`
- `https://codeload.github.com/armink/EasyLogger/zip/806328e131836662fa83dd43364a53f699fd76ac`

归档 SHA256：

```text
CherrySH   45316d2517dfc470461c08141b5578954a6131d56a61d30c9ac4d8c65a833e93
EasyLogger 8836303efe2a90afe3081fbedb106129f63c7092206cf0995e81f8ff8cc82fe0
```

官方来源：[CherrySH 提交](https://github.com/cherry-embedded/CherrySH/commit/c8c1c0964bc0033f24efb255cd607b35e3a19490)、[EasyLogger 提交](https://github.com/armink/EasyLogger/commit/806328e131836662fa83dd43364a53f699fd76ac)。完整上游目录位于 third_party，未修改、不入库。

## 构建与资源

本次先执行原 Debug/Release 构建，均成功（原构建树无待编译工作）；读取原 ELF 尺寸作为基线。加入依赖与空命令表后重新构建成功。

| 固件 | 基线 Flash | S0 Flash | 基线 RAM | S0 RAM |
|---|---:|---:|---:|---:|
| REV6-Debug | 54764 B | 54764 B | 27904 B | 27904 B |
| REV6-Release | 26824 B | 26824 B | 27904 B | 27904 B |

RAM 为 data+bss（148+27756 B），包含现有堆、静态栈及链接器预留，不是运行时堆/栈剩余。Flash 限额仍为 126 KB，参数页和应用地址未改变。

两个静态库独立参与默认构建，尚未加入固件 target_link_libraries；固件中没有 chry_shell/chry_readline/elog 运行符号。库大小不计作已启用功能的固件占用。未创建新任务或运行时资源，未测新栈水位。

## S0 检查结果

| 检查 | 结果与证据 |
|---|---|
| 首次获取 | Debug 配置自动获取两个缺失目录，CMake 校验固定归档 SHA256 |
| Debug/Release | 固件与两个静态库均编译成功；无残留编译警告 |
| 离线配置构建 | 两种 preset 均以 FETCHCONTENT_FULLY_DISCONNECTED=ON 配置、构建通过，随后恢复 OFF |
| 源码目录缺失 | 两个库在隔离夹具中均明确失败，不尝试联网 |
| 必需源码缺失 | 两个库均在配置阶段明确失败 |
| 源码改变/混用版本 | 两个库均以哈希不一致拒绝，不覆盖已有文件 |
| 编译源清单 | compile_commands 中新增恰好四个核心源，无上游示例、默认 clear/login 或异步线程 |
| 配置选择 | EasyLogger Debug 定义 FOC_LOG_DEBUG_BUILD=1，Release 无该定义；项目配置优先于上游同名文件 |
| 命令段 | ELF 全局符号中四个边界存在，4 B 对齐，start=end；本阶段无命令注册 |
| 运行时隔离 | 固件无新库运行符号，Flash/RAM 零增量 |
| C23/C++23 | REQUIRED、关闭扩展；现有所有 C 目标按 C23 重编成功。独立 OBJECT 目标验证 C23 typeof_unqual/nullptr/static_assert、C++23 if consteval/多参数下标及公共 BSP 头，编译成功 |

最终编译清单核对：每种固件均有 48 个 C 翻译单元使用 C23 模式。完整下载目录另与固定归档逐文件比较，CherrySH 81 个文件、EasyLogger 615 个文件全部一致；结果保存在 build/shell-log-stage0/upstream-integrity.json。此完整树比对是本阶段额外实际检查，不是每次 CMake 配置都会执行的检查。

缺失/改变源码场景使用 build 下的最小隔离副本，不修改实际下载的上游源码。已有目录逐文件校验只覆盖记录的编译输入/许可证，不声称每次配置检查完整上游示例树。

本地脚本：`tests/check_shell_log_stage0.py`，当前覆盖 6 个失败场景和 2 个固件编译清单/符号场景。夹具输出和 configure.log 位于 `build/shell-log-stage0/checks/`，汇总 `results.json`。下载归档和供对照的解压副本位于 `build/shell-log-stage0/`。上述均忽略、不入库。

### 适配发现与处理

- 上游 CherrySH 调用 strnlen，严格 C/Newlib 默认隐藏其声明，首次 C11 编译因此失败。只给 cherrysh 目标增加 `_POSIX_C_SOURCE=200809L`，启用工具链已有 POSIX 声明，随后 C11 和升级后的 C23 两构建通过；未修改上游、未降低隐式声明检查。
- C23 移除了 ATOMIC_VAR_INIT，升级后 app_device_state.c 编译报错，已改为 `static atomic_uint stateWord = SYSTEM_STATE_INITIALIZING;`，保留原有静态初始化语义。之后全目标 Debug/Release 构建通过，资源与原基线一致。
- 当前 CMake 3.31.4 对 GCC 14.2.1 的 C23 选项是 `-std=c2x`，GCC 帮助确认其与 `-std=c23` 同义；C++ 使用 `-std=c++23`。语言检查与日志位于 build/shell-log-stage0/language-probe，不宣称标准特性全部实现。
- 仅 CherrySH 目标关闭上游未使用参数警告，未影响项目目标。
- EasyLogger 保留一个内建 tag 等级槽位以满足上游固定数组结构，本次不开放模块过滤；关键词/标签功能未通过业务接口启用。
- EasyLogger 内建格式与目标 `[秒.毫秒] I tag: text` 不完全一致，S3 由项目适配完成，不把当前配置误报为格式已实现。
- 静态库内的命令对象不会单凭 KEEP 自动被提取。S2 把上游 help.c 直接加入固件源码，项目命令也直接加入固件；本阶段只预留符号。

## 文件一致性

本次对应计划 S0（含用户追加的语言标准升级），共修改/新增 17 个入库候选文件：

- 修改：`.gitignore`、`CMakeLists.txt`、`STM32G431CBUX_FLASH.ld`、`Docs/project_memory/README.md`、`project/middleware/third_party/README.md`；语言升级另改 `cmake/gcc-arm-none-eabi.cmake`、`project/application/state/app_device_state.c`、`Docs/project_memory/04_architecture.md`、`07_interface_rules.md`。
- 新增：`cmake/cherrysh.cmake`、`cherrysh.sha256`、`easylogger.cmake`、`easylogger.sha256`；`project/porting/cherrysh/csh_config.h`、`project/porting/easylogger/elog_cfg.h`；本计划与本验收记录。
- 下载目录：CherrySH、EasyLogger 完整上游树，均被忽略。
- 本地检查：tests 脚本、build 夹具与归档，均被忽略。

用户原有 `Docs/project_memory/03_freertos_clion_debug.md` 修改保持原样，不属于本次差异。没有创建 S1 的 app_usb_task.h；app_task.h 仍只放任务入口。未修改 main、USB 任务、BSP、旧解析器和设备驱动。

修正后的最终目录已写入计划：新增 USB 输出接口放 app_usb_task.h，不放 app_task.h；不新增 Console 文件或任务。

最终 Git 清单检查通过：排除用户原有调试文档修改后，17 个修改/新增文件均在计划最终目录中列出。

## 后续实板结果

（S0 时点历史记录）S2–S3 当时未实施。三轮 Debug/Release/Debug 的 root、命令、编辑、日志、串口重开、reboot/dfu、栈水位均未执行。S1 主机测试不替代最终实板三轮回归。

## S1 实际交付（2026-10-31）

本阶段仅修改 main.c、app_usb_task.c，新增 app_usb_task.h，并更新本计划/验收记录。无新增任务、Console 文件、BSP 或底层 USB 改动；CMake 无需增加源项。旧 app_usb_console 继续使用，CherrySH/EasyLogger 未链接或初始化。

### 接口与数据归属

| 接口 | 契约 |
|---|---|
| App_USBOutput_Init(task) | 启动任务在 USB 任务创建之后、CDC 初始化前调用一次，绑定有效任务句柄 |
| App_USBOutput_Stop() | 关闭提交并作废未发送日志；CDC 初始化失败时先调用，再删任务；reboot/dfu 接受后也调用 |
| App_USBOutput_LogWrite(data,length) | 普通任务复制完整 1–192 B 记录；满/关闭/长度无效返回 false 并计丢弃，不等待 USB |
| App_USBOutput_DroppedLogs() | 查询满队列、关闭、长度无效、失败/会话作废等累计丢弃条数 |
| App_USBOutput_Write(data,length,completed) | 仅 USB 任务调用；普通输出共享 100 ms 上限；返回状态及已收到 TX_DONE 的字节数 |

任务专用头需要 FreeRTOS 的 TaskHandle_t；未来业务 LOG 调用者不需包含该头。App_USBTask 的入口声明仍在 app_task.h。

环形存储总计 1024 B，每条使用 2 B 长度头和正文。例如 5 条 192 B 日志加一条 52 B 日志恰好占满，正文总计 1012 B。正在发送的记录仍占队列空间，生产者不能覆盖；跨尾部以两个片段共用一个 100 ms 截止时间，无额外记录暂存区。

生产者复制/更新与通知同处短临界区，与 Stop 串行，避免删除任务后通知旧句柄。CDC 等待、时间等待均不在临界区。每轮最多发送一条日志后重新检查输入。

普通响应改用有限等待；reboot/dfu 保留最多等待 200 ms 的契约，断线或发送失败后仍执行动作。发送超时不清除 txPending，只有 TX_DONE 或 SessionId 改变才允许新的物理提交；迟到完成不会让任务重复提交旧数据。

输出失败后丢弃待发日志、清理旧解析状态；本阶段没有 Shell 提示符可重绘，S2–S3 才接入编辑会话重建。初次连接前暂存保留，已建立连接后的会话变化清理旧记录，不跨会话重放。

### S1 检查结果

统一入口：`python tests/check_shell_log_stage1.py`。脚本、替身和产物均本地忽略。输出在 `build/shell-log-stage1/`，包括 results.json、每项日志及可执行文件。

| 检查 | 结果 |
|---|---|
| 新输出状态机 | 24 个短时主机场景通过，直接运行实际 app_usb_task.c，不访问目标板 |
| 旧协议回归 | 原文本边界/CRLF/溢出、任务分包/复位/DFU/会话、ROM 交接与 BSP 平台主机测试通过 |
| 启动回归 | 实际 main.c 测试通过；确认 Init 在 CDC 前，失败时 Stop 在任务删除前 |
| 公共头 | 使用实际 FreeRTOS/CMSIS 头的 Cortex-M4F C++23 独立语法检查通过 |
| 格式 | 三个生产代码文件 clang-format 检查通过 |
| 构建 | C23 Debug/Release 均成功，无新增编译警告 |
| 实板与调试器 | 未执行；无烧录、复位、CLion 会话或新栈水位实测 |

24 个输出场景覆盖：包边界 1/63/64/65/127/128/192 B；超过队列容量的 1025 B 普通输出；空输出；BUSY 和临时 NOT_READY；100 ms 超时及迟到完成；只报告已完成字节；tick 回绕；提交和 IRQ 错误；会话取消；记录复制与顺序；整条拒绝及精确容量；长度边界；长度头/正文跨尾部；发送中生产者追加；跨尾记录共享截止时间；失败清队列计数；关闭后不通知旧任务；START 前日志唤醒；休眠边界入队；输入与日志交替；旧会话记录清理；裸 CR 超时响应后的待接收包。

这些是单进程内核/CDC 替身的确定性调度检查；发送期间追加记录模拟交错，不宣称已验证真实多任务抢占或实板 USB 时序。

### 发现并修正的等待边界

裸 CR 超时生成响应时，该轮没有消费新输入。发送等待可能同时取走下一包 RX_READY；若发完立即永久等待，会留下未消费的 CDC 包。新用例先复现失败，再修正为处理过响应后先进入下一轮检查实际 RX 缓冲，不直接休眠；用例通过。旧多包连续输入回归也通过。

### S1 资源与目录核对

| 固件 | S0 Flash | S1 Flash | 增量 | S0 RAM | S1 RAM | 增量 |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 54764 B | 55768 B | +1004 B | 27904 B | 28960 B | +1056 B |
| Release | 26824 B | 27524 B | +700 B | 27904 B | 28960 B | +1056 B |

RAM 剩余链接空间 3808 B，不等于运行时可用栈/堆。USB 静态任务栈仍为 1024 B，计划 S2 调整至 2048 B；本阶段 GCC .su 显示 App_USBTask 单帧 72 B、SendLog 48 B、SendBytes 40 B，不能据此替代完整调用链栈水位。后续仍需核对 Shell 与日志的总预算。

阶段 0 的文件继续保留；S1 新增/修改的三个生产文件和两份文档均已列入计划最终目录。没有改动用户既有调试文档，没有提交或推送。等待用户审核后进入 S2。


## S2 增量：提示符颜色与参数补全（2026-10-31）

实际 CherrySH 主机测试 28 个场景通过，包含原有 23 场景以及 system time/uid 参数补全、test enc 参数补全、无匹配参数、clear 后补全恢复。主机表仍为五命令替身，test 补全只检查编辑缓冲，不代表编码器命令执行验证。

Debug 构建通过：Flash 73716 B，RAM 30496 B。仅新增修改 app_shell.c 的提示符样式和应用补全回调，未修改第三方源码。新固件颜色和补全尚未实板验证；EasyLogger 与启动前日志未实现。


## S3 实际交付（2026-10-31）

用户已确认 S2 几个 Shell 命令实板测试；本阶段未烧录或操作调试器。EasyLogger 已链接并启用，后续等待用户日志验收；最终 S4 三轮短时实板回归未执行。

### 实现与边界

main 在启动任务创建前初始化日志，记录时钟/DWT 与 LL IO 就绪；记录保存在原 1024 B 队列。绑定 USB 任务只绑定消费者，不清空队列/丢弃统计。绑定前严格只有 main/启动任务生产，不接受其他任务或 ISR；此时不调用队列 RTOS 临界区。绑定后使用可嵌套的 FreeRTOS 临界区，生产者仅复制记录、通知，不等待 USB。

静态 FreeRTOS 互斥量串行保护 EasyLogger 格式化和运行时配置。main 将锁、解锁、ISR 检测函数作为 App_LogPlatform 注入应用门面；app_log 不引用 porting 头，移植仅调用 main 注入的输出/时间回调，不反向依赖应用或 BSP。未改第三方源码、端点、BSP CDC 或任务数。

默认 INFO，颜色默认开。Debug 的 `log level`/`log color` 可查询，带参数可设置；`log stats` 显示当前配置与 emitted/filtered/truncated/dropped。Release 无 log 命令且 LOG_D 参数不求值。启动前时间显示 `[0.000]`，调度后示例 `[1.264] I usb: CDC initialized`；不使用浮点。消息正文最长 143 B，超长以省略号结尾并计 truncated，标签显示前 16 B；最终记录最长 192 B。

启动记录先缓存至首个 CDC 就绪，未连接主机时不阻塞启动，不保证在终端尚未打开时已经由主机接收的数据仍可回看。初始化资源结果和状态/故障变化记录日志，不周期刷屏。`log level debug` 后执行 `test enc` 可观察单次读取的 DEBUG 记录；命令响应先完成，然后消费日志。

日志发送前清除显示行，发送后恢复 root 提示符、编辑行和光标，不清历史或分包 ESC 状态；失败走现有显示失效/会话重建路径。CLI 不向日志记录其自身的收发错误，避免递归。

### 实际检查

统一入口 `python tests/check_shell_log_stage3.py`，产物 `build/shell-log-stage3/`；测试源码与产物本地忽略，不入库。

| 检查 | 结果 |
|---|---|
| 实际 EasyLogger + 项目移植/门面，Debug/Release | 均通过：早期无调度输出、精确格式/颜色、OFF 参数跳过、阈值、Release DEBUG 消除、长行和最大时间、拒绝统计、ISR 拒绝；各运行四个主机线程并发提交 400 条 |
| 实际 CherrySH + 项目命令 | Debug 36、Release 35 个场景通过，含编码器单次读取/失败/参数、日志重绘光标、分包 ESC、显示失败恢复、Debug 控制参数与 Release 不存在 log |
| 实际 USB 输出状态机 | 21 个场景通过：原有边界/超时/迟到完成/回绕/环形跨界/容量/关闭，加上调度前提交后绑定保留、嵌套临界区保持 |
| 实际 USB 任务 + CherrySH | 7 场景通过：START 前缓存、日志与多命令交替、会话丢弃、reboot/dfu 单次动作，以及确认不完成时的 200 ms 时限 |
| 实际 main.c 启动回归 | 通过：日志初始化和两条早期记录发生在创建任务之前；原初始化失败/资源故障路径保留 |
| 构建与固件符号 | Debug/Release 通过；实际 ELF 中 Debug 注册 log、Release 不注册且不编译 cmd_log；旧解析器无构建输入/符号 |
| C++23 头文件 | app_log.h、app_shell.h、elog_port.h ARM 编译器独立语法检查通过 |
| 实板/栈水位 | 未执行；主机替身并发不等于真实 FreeRTOS 抢占/USB 时序验证 |

### 资源与目录

| 固件 | Flash | RAM | 剩余链接 RAM |
|---|---:|---:|---:|
| S3 Debug | 81648 B | 30960 B | 1808 B |
| S3 Release | 41984 B | 30912 B | 1856 B |

相对 S2 Debug 的 73716/30496 B，Flash +7932 B，RAM +464 B。无第二个日志大缓冲、无新增任务；USB 栈仍为 2048 B。GCC Debug 单帧报告 App_Log_Write 208 B、elog_output 96 B、App_USBTask 72 B、SendLog 56 B；这不是完整调用链，也不替代任务栈水位。链接余量不是运行时可用堆/栈。

S3 首次交付时累计的 36 个新增/修改/删除文件已核对均列入计划最终目录；用户原有 03_freertos_clion_debug.md 修改不属于本次交付，未改动。新增 app_log.c/.h、elog_port.c/.h、cmd_log.c；其余 S3 修改入口/任务/会话/配置已在目录标明。未提交、未推送。


## S3 首连日志修正（2026-10-31，当前版本）

用户实测 `emitted=6 filtered=1 dropped=6`，确认初版生成了启动日志但全部丢弃；手动开启 DEBUG 后 `test enc` 日志、OFF 控制工作正常。初版默认 INFO；按最新要求现改为 Debug 默认 DEBUG，Release 默认 INFO。

代码原因：IsConfigured 只表示枚举就绪，不能表示终端已经打开；初版在尝试输出提示符前设置 sessionEstablished，超时便清空启动队列。现通过 DTR/实际 OUT 数据标记终端就绪，首次提示符成功前的发送失败或初始总线复位保留启动队列。已建立会话之后的丢弃策略保留。终端不切换 DTR 时，发送任意字符或 Enter 才开始显示。

检查入口仍为 `python tests/check_shell_log_stage3.py`，全部通过：原有日志、Shell、21 场景输出状态机、启动回归、Debug/Release 构建与符号、C++23 头检查继续通过；集成场景由 7 增至 10，新增配置完成后等待终端、首个提示符 100 ms 超时后迟到完成保留启动日志、首个提示符期间总线代次变化保留启动日志。真实 CherryUSB core/class 与 BSP/porting 的 cdc-terminal 测试通过，包含 DTR 开/关、OUT 数据兼容唤醒与枚举/收发回归。Debug 默认等级另有主机断言。

当前固件：Debug Flash 81848 B、RAM 30960 B；Release Flash 42104 B、RAM 30912 B。无新增任务或大缓冲。累计修改目录覆盖 40 个交付文件，新增四个修改位置是 BSP CDC .c/.h 与 porting CDC .c/.h，未改第三方源码。

本修正未烧录，DTR 与终端实际打开时序仍待用户重测，不能把主机结果写为实板通过。建议重启/重新枚举后连接 COM11，必要时按 Enter，观察启动日志及 `log stats` 默认 debug；没有拥塞或断线时不应再出现初版六条整批丢弃。


## 用户实板反馈与中文帮助/补全增量（2026-10-31）

用户提供七条启动日志（含默认 DEBUG 提示）、多次 test enc 的运行 DEBUG、system uid/time、log color off，最终统计 emitted=11、filtered=0、truncated=0、dropped=0。首连日志丢弃修正确认在该次用户实板操作中有效。`test level off/debug` 属于错误命令；正确为 `log level off/debug`，现有参数校验拒绝错误输入，日志仍保持 DEBUG 属正常结果。

新增 log 中文简介、详细中文帮助与统计字段解释；补全覆盖二级 level/color/stats 和三级等级/颜色值；空参数 Tab 调用公开 readline 完成接口，而非上游的缩进路径。不改第三方代码。

统一检查入口再次通过：Debug 实际库 Shell 45 场景、Release 36 场景（含 Release 不提供 log 候选）、日志门面两个构建、21 场景输出状态机、10 场景集成、启动回归、实际 CDC core/class、C++23 头及两构建符号。新增检查含中文帮助完整内容、二三级补全、空参数候选、OFF、无效多余参数。共同前缀按库规则先填充，再次 Tab 列出候选。

Debug Flash 82596 B、RAM 30960 B；Release Flash 42156 B、RAM 30912 B。没有增加静态 RAM 或任务。新中文帮助/补全尚未实板测试；没有烧录/复位，最终三轮回归仍未执行。最终目录保持累计 40 个交付文件，本轮仅修改已列出的 app_shell.c、cmd_log.c 与本轮两份文档。


## S4 三轮短时实板检查（2026-10-31，最终交付）

用户授权执行下一阶段并断开 MobaXterm 的 COM11。仅执行既定 Debug → Release → Debug 三轮完整串口回归，不运行旧的 600 秒持续回显脚本。S4 未修改生产代码；目标为 STM32G431CB，UID `00210040484E500220333031`，USB Composite Device 的实际实例 ID 同为该 UID。

### 每轮结果

| 项目 | 第一轮 Debug | 第二轮 Release | 第三轮 Debug |
|---|---:|---:|---:|
| 串口主检查 | 31 项通过 | 24 项通过 | 31 项通过 |
| 应用恢复检查 | 通过 | 通过 | 通过 |
| 串口关闭/重开 | 通过 | 通过 | 通过 |
| 命令 reboot / 重新枚举 | 通过 | 通过 | 通过 |
| dfu / ROM 0483:DF11 | 通过 | 通过 | 通过 |
| USB 下载同轮 ELF / verify / GO | 通过 | 通过 | 通过 |
| 有界预置拥塞 / 完整记录 / 恢复 | 通过 | 通过 | 通过 |
| 参数页烧录后/恢复后哈希 | 一致 | 一致 | 一致 |
| startup 最低未使用栈 | 1260 B | 1280 B | 1260 B |
| USB 最低未使用栈 | 856 B | 1020 B | 816 B |
| LED 最低未使用栈 | 1852 B | 1848 B | 1852 B |

主检查覆盖 root 免密码、绿色加粗提示符的 ANSI 字节、启动记录、帮助、UID/USB 序列号、整数毫秒时间、CR/LF/CRLF 去重、退格/DEL/删除/光标插入/历史、clear 清屏及清历史、system/test 补全、编码器单次读取、错误参数拒绝、超长行拒绝和恢复。Debug 另检查中文 log 帮助、默认 DEBUG/颜色、青色 DEBUG、OFF 后命令仍响应但不输出日志、恢复 DEBUG、color off、level/color/stats 二三级补全与统计。Release 确认帮助无 log、log 为未知命令且 test enc 不产生 DEBUG。

三轮静态栈均为 2048 B，通过暂停后直接导出静态栈数组，从底部扫描连续 0xA5，取各轮已保存快照的最小未使用字节数；startup 删除后静态栈不释放，初轮另在 LED 首次入口保存。最低值高于 256 B 目标。单帧编译报告未用来替代水位；暂停不与正常 USB 时序测量混用。

### 有界队列检查的方法

最初尝试在 main 断点通过 GDB 调用入队函数；第一次字符串参数因无 malloc 无法求值，随后使用静态缓冲参数的注入未返回，服务器日志显示注入写入 SP=0。已停止该 GDB 客户端并复位恢复，不将该次注入视为产品功能结果，也不据此宣称固件 HardFault。

正式三轮均改为 main 入口（无任务、无消费者）预置六条合法队列记录：5×192 B + 1×52 B 正文与六个 2 B 头，总占用恰为 1024 B。通过调试器写入测试状态，然后运行实际 main/启动任务和 EasyLogger，使真实启动日志触发队列满拒绝路径；未执行目标函数注入。Debug 实测 used=1024/count=6/dropped=7，Release dropped=6。导出队列与预置 1024 B 逐字节一致；终端打开后六条 QTEST 记录各出现一次，排空后 test enc 与 Shell 正常。Debug log stats 核对 dropped=7，Release 通过内存快照核对 dropped=6。

这是有界且受控的满队列/整条拒绝/统计/恢复验证，不是连续真实多生产者负载测试。原 21 场景主机输出边界检查继续提供跨尾部、迟到完成等证据；未开展长时间压力或 FOC 实时性验证。

### 参数页保护与恢复

初次烧录前通过 SWD 备份 `0x0801F800..0x08020000` 的 2048 B，每轮下载 ELF 后和 DFU 恢复后导出比较，全部一致：

`SHA256=d0ff1b294b5288d1ae1421eadf5b2d38a8752b76d472ff30bed9028e25b1c5b8`

GDB load 只下载 ELF 段；ROM DFU 使用 STM32CubeProgrammer USB1 下载同轮 ELF、verify、GO。三轮均有 Download verified successfully。未执行整片擦除、参数写入或编码器 EEPROM 写入。

### 调试工具限制与恢复动作

已按 clion-debugger 技能读取 CLion MCP 状态、运行配置和断点。启动工具长时间未返回，后续状态仍 sessions=[]；没有启动有效 CLion 调试会话。回退到现有 J-Link GDB server + arm-none-eabi-gdb，不修改用户 IDE 断点。

第一轮 ROM DFU 下载后直接 GO，应用 UID/Shell 正常，但持久服务器读到 0xdeadbeee/无法访问内存，重连及 1 MHz、复位连接仍报 DAP 初始化失败。通过应用 reboot 普通复位后，SWD 恢复。第二/三轮在 ROM GO 后增加一次普通 reboot 恢复调试，再读内存；这些是调试链路恢复动作，不另计完整回归轮数。没有证明根因，不把它归为应用 HardFault。手动断电曾作为待选恢复动作询问用户，随后软件复位恢复，未依赖用户断电。

测试中还修正了本地 PowerShell 脚本的 UTF-8 BOM、枚举等待和一字节状态枚举读取方式；这些不是生产代码修复。早期各阶段共用 serial.txt 被后续阶段覆盖，保留的 JSON/工具输出为主检查判据，后续脚本改为按阶段保存原始串口文本，未为补齐文本再跑第四轮。

### 产物、资源和最终状态

本地入口：`tests/shell_log_stage4_serial.ps1`。产物：`build/shell-log-stage4/`，包含 summary.json、每轮 serial/recovery/congestion-results.json、GDB 脚本/记录、队列前后快照、栈快照、参数页快照和 DFU 下载日志。本地 tests/build 按既定规则忽略，不入库。

Debug Flash 82596 B / RAM 30960 B，Release Flash 42156 B / RAM 30912 B；S4 无生产代码资源变化。累计 42 个交付文件已核对最终目录，排除用户原有 03_freertos_clion_debug.md 修改。依赖提交与哈希仍由 CMake 清单固定，第三方源码未改。

清理了拥塞种子并普通复位，目标最终运行正常 Debug。`final-sanity.txt` 确认正常启动及 `level=debug color=on emitted=7 filtered=0 truncated=0 dropped=0`。COM11 已关闭释放，代理创建的 J-Link server PID 25280 已停止；CLion 最终 sessions=[]，未改用户断点。没有 Git 提交或推送，停止并等待用户最终评估。
