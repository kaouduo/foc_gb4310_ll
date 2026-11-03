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


## FOC 台架验证授权与阶段 1 追加（2026-10-31）

用户明确表示当前 24 V 台架可直接测试 PWM，硬件已验证，与参考工程接近的控制算法可直接验证。因此，本台架后续同范围测试不再反复要求先断开 U/V/W 或重新确认授权；新增超出已讨论模式/幅值/持续时间的试验仍需按实际任务确定范围。

已实际执行 test pwm u/v/w/all，各约 1 s，20000 个目标样本，无采样/截止错误，结束输出关闭；当前 Debug、串口释放，未 Git 提交。具体证据和最终清单见 13_foc_stage1_validation.md。原有硬件示波器结论由用户提供，本次代理没有直接采集示波器波形。


## FOC 阶段 2 当前边界（2026-10-31）

开放有界开环电压：motor open 默认 2 Hz/1 V/3 s/200 ms 上下斜坡；motor stop 同步关断，motor status 读取冻结/实时快照，motor clear 显式清本模块故障且不重启。新增 1 ms 静态 motor 任务（优先级 3、2048 B 栈），自动零偏、母线动态限幅和快速电流/时效保护。运行期间 test enc/test bus 返回缓存，采样/PWM 诊断拒绝并发。

已定位并修复 supervisor 当前时间与 ISR 最后采样时间读取竞争导致的假超时，含可控中断插入回归。电角度/极对数校准、Flash 标定和实机电流环调参仍未实施；当前结果和最终实际目录见 14_foc_stage2_open_voltage.md。未经授权不提交或推送。


## FOC 阶段3最终状态（2026-11-01）

motor cal start/status/stop已实现：2 Hz/5 V，初始与约120°电流检查、正反各一机械圈、1～64极对自动辨识、方向/全圈零偏、质量门禁与停机自动保存。使用原任务、唯一开环对象及快速保护；普通open仍最多10 s，硬件租期上限90 s供最坏84 s校准。单页64 B记录版本2，含schema/CRC/最后提交标记，启动加载不自启；取消/质量失败不覆盖旧记录，单页写失败不保证旧值回滚。

实板探索后：排除前100 ms暂态点，拟合RMS门槛0.35 rad；全圈正反圆周均值计算零偏。每圈结束回初始电角度，检查双向静态停靠中心相对初始零位≤0.25 rad、端点跨度≤0.5 rad；不是要求两端完全重合。参考工程也使用停靠中心抵消迟滞。不硬编码14极对，不将本轮RMS或重复性当成绝对精度，未做编码器非线性查表补偿。

最终Debug→Release→Debug三轮实板通过，均辨识14极对/方向−1，零偏4.468～4.469 rad，约17.4 s完成，峰值≤0.880 A；完整保存、重启加载不自启、取消、停止和重复启动门禁通过，ADC/截止错误0。IRQ最坏2099 cycles（12.347 μs）。主机O0/O2覆盖轨迹、迟滞、失败路径与存储损坏。Debug RAM23696 B/Flash112276 B；Release23648 B/73444 B。最低堆6008 B；实测最少未触及栈USB556 B、motor872 B、LED1208 B。

最终Debug板端代码及Flash .data初值已核对，输出/采样关闭，COM11释放，没有新建或关闭用户既有J-Link服务器，无GDB批处理遗留。完整证据、参数SHA和累计62文件清单见15_foc_stage3_calibration.md。最终验收用户评估，实机编码器电压和电流模式尚未开放。不提交Git，原03文件改动保留。


## FOC全部变动审查与规范整理（2026-11-01）

用户要求检查全部变动、注释/格式、冗余设计与诊断过度。本轮补充48个自有C/H文件的接口契约与单位，收紧.clang-format（控制花括号、展开函数体），统一UTF-8/LF；没有改控制参数、功能逻辑、现有命令或对象布局，没有烧录/操作电机，没有提交。Debug/Release与主机回归通过，资源不变；03用户原有改动哈希不变，第三方只读。

审查发现并记录待整改：P1裸test pwm没有正常运行的电流/母线保护且Release可用；P2生产零偏依赖完整Diagnose，729项软件自检及详细诊断进入Release，Stop/RequestStop重复停止且Finish临界区内可能等ADC停止1ms，test foc固定output=off与实际状态不一致、PWM结果占空比记录可能陈旧。可精简mode预留实例/表124B和只写last_id/iq8B；FOC算法/基类保留用户已确认设计。TIM1专用实现不应伪装通用timer接口。不要把保护、CRC、截止检查当作冗余删掉。

完整审查/整改建议与累计实际清单见16_foc_review.md。本轮只做规范整理，未声称上述功能问题已修复；板端仍是阶段3实测版本，新的上板核对需在实际功能整改后安排。优先补带电诊断保护、收敛停止事务，再拆生产零偏与诊断，不同时重写整个执行器。


## FOC 审查整改（2026-11-01）

用户已授权修复。PWM 诊断经运行执行器复用保护，生产采零不调用测试入口，Debug/Release 的高级诊断实现/命令/帮助/补全同步开关；停止所有权认领与 ADC 等待分离，取消准备由采样所有者收尾。移除预留实例和无消费者 dq 缓存、假通用 timer 配置，算法源码保留。校准记录格式不变。详见17_foc_fixes.md；当前实板回归等待用户断开被占用的COM11，不能引用旧阶段3结果作为本次通过。


## FOC 整改三轮实板完成（2026-11-02）

用户释放COM11后完成Debug→Release→Debug三轮（28/27/31项）。首次关闭输出chain截止错误定位为diagnostics.c漏-O2，加入既有优化列表后正式三轮通过；Windows GDB与PWM等待问题只修测试工具。三轮14极对、方向−1、零偏4.461/4.468/4.467rad，校准峰值≤0.898A、IRQ≤2081cycles。生产采零、开环、取消、保存、重启及Release诊断开关通过；第三轮PWM U完成20000帧无硬件错误。最终Debug RAM23576B、Flash105652B，Flash可加载节读回一致，8B非加载空洞擦除态。当前输出/采样关闭、串口释放、未Git提交，见17_foc_fixes.md，覆盖前述待验证状态。


## VOFA UART3 DMA 已接入（2026-11-02）

用户明确VOFA走USART3而不是USB CDC；已移植参考工程纯JustFloat组帧，BSP静态绑定PC10 AF7、USART3/PCLK1、DMA1 Channel1/DMAMUX request29，完成/错误IRQ优先级6，无RTOS调用。新增持久静态vofa任务1ms检查、优先级1、1024B栈；成功FOC控制周期分频发布16通道同周期快照，采样IRQ不发送。COM9实测高波特率配置丢字节，最终115200/100Hz（20kHz每200帧）三轮Debug→Release→Debug各99帧/6732B通过，DMA提交/完成/接收数量一致，忙/错误/跳过均0。COM11继续Shell和日志。当前开环FOC输入id/iq仍0，不伪装实际dq反馈。Debug RAM24880B Flash109180B，Release24816B/65444B。当前保留Debug，输出/采样关闭、COM9/COM11已释放，未提交。通道、实现边界、失败探索和最新累计目录见18_vofa_uart_dma.md；不重做长测。

收尾追加：用户确认保留其并行修改的foc_svpwm.c，仅添加缺失stddef.h；主机43,230向量及控制/校准/存储回归、双构建通过。最终Debug Flash109180B与当前ELF导出（FF填空洞）逐字节一致，未再改变SVPWM逻辑。


## 四阶段 Git 交付（2026-11-03）

用户已授权按四阶段提交、绑定GitHub kaouduo/foc_gb4310_ll并推送main。共享文件按阶段生成源码版本，每笔均完成Debug/Release隔离导出构建；阶段1算法、阶段2开环/故障/停止、阶段3校准与存储主机检查通过。最终实现保持一致，此次Git整理未烧录或运行电机。前三笔为 f29855b, 32992ae, 4d0c320，第四笔为本次USART3 DMA/JustFloat遥测提交。03_freertos_clion_debug.md独立修改保留且校验未变；tests/build/IDE及Obsidian外部文档不入此仓库。远端目标https://github.com/kaouduo/foc_gb4310_ll，同步状态以git status/remote为准。
