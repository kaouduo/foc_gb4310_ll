# CherryUSB 分阶段实施与验收计划

更新日期：2026-10-31（Asia/Taipei）。状态：阶段 0–4 已经用户功能验收；阶段 5 自动联合验收完成，人工拔插按用户要求取消，等待最终审核。实板结果以验收记录为准。

## 范围与执行规则

实现 CherryUSB CDC、文本回显和 `uid`、`reboot`、`dfu`。USB 持续连接电脑时，普通复位后重新枚举；软件进入 ST 出厂 DFU 后可下载并恢复运行。不引入 shell 库，不自建 Bootloader，不改应用起始地址和末尾参数页。

每阶段只实施当前阶段；完成后报告文件差异、核验结果、规范例外和待验证项，停止等待用户验收。整改后重新交付，不能自行进入下一阶段。未经单独授权不提交、不推送。

当前源码事实、参考配置和硬件实测必须区分。实际结果统一记录到 [USB 验收记录](09_usb_validation.md)，未验证不能写成通过。

## 职责与接口

| 位置 | 职责与边界 |
|---|---|
| system_clock | 系统主频、HSI48、USB 48 MHz 时钟源、CRS；不处理 USB 引脚或描述符 |
| usb_io | 引脚输出低及恢复；配置由 BSP 传入，不配置时钟树、不包含 BSP 头 |
| usb_dc_port | USB 外设门控、复位、IRQ 和 CherryUSB 低层钩子；不配置 HSI48/CRS、不解析命令 |
| usb_cdc / usb_descriptors | 端点、缓冲与描述符；不读取 UID 寄存器，不反向调用 BSP |
| BSP_USB_CDC | 板级参数、初始化时序、UID 传入及应用可见收发接口 |
| BSP_Platform | 通过现有 UniqueId、Reset 及新增 EnterDFU 提供平台能力 |
| application | main 可直接编排 porting 初始化；其他任务通过 BSP 实现行解析、命令语义和调度 |

保留现有 `BSP_Platform_UniqueId()` 和 `BSP_Platform_Reset()`，新增 `void BSP_Platform_EnterDFU(void)`，成功后不返回。BSP 通过 UID 接口生成 24 位大写十六进制序列号，再向描述符适配传入。

USB 公共能力为初始化、配置状态查询、非阻塞读取、非阻塞写入及 RX/TX/会话事件通知。Write 成功只表示内部接受发送，不表示主机应用已读取。公共头使用 Sys_Status、普通 C 类型和 C++ 链接保护，不暴露 LL、CherryUSB 或 FreeRTOS 类型；事件明确标注上下文。

### 文本协议

| 完整输入行的正文 | 响应与动作 |
|---|---|
| `uid` | `UID=<24位大写十六进制>\r\n` |
| `reboot` | `OK reboot\r\n`，发送完成或等待 200 ms 后普通复位 |
| `dfu` | `OK dfu\r\n`，发送完成或等待 200 ms 后进入 ROM DFU |
| 其他文本或空行 | 原样回显，保留原始结束符 |
| 正文超过 128 字节 | `ERR line too long\r\n`，丢弃剩余内容到行结束，再接受下一行 |

支持 CR、LF、CRLF，CRLF 只结束一行。命令区分大小写，整行精确匹配，不裁剪空格。采用静态行存储，正确处理分包、合包及端点背压；为保留原始结束符，CR 待定状态允许等待后续输入或短暂行尾判定超时（默认 10 ms）。裸 CR 超时后结束一行，随后紧邻到达的 LF 仍只作为配对结束符处理，不产生第二条命令。回显按行执行，不承诺任意二进制透明传输。

命令和响应串行处理；已接受 reboot/dfu 后，即使响应超时或断开也执行，不因等待主机读数据永久卡住。普通 USB 会话重置清除未完成文本和旧传输状态，不跨连接重放。

## 阶段 0：基线与文档

- 建立本计划、带功能边界注释的最终目录树和验收记录，更新索引。
- 检查当前源码、参考工程、构建及存储布局，记录 USB/BOOT 实板待确认项。
- 仅只读枚举调试器和 USB；不复位、烧写或修改选项字节。
- 核验：Debug/Release 构建成功，尺寸可复查；硬件未知项明确记录。
- 规范：文档集中在 project_memory，不提前下载依赖或修改固件。
- 交付后停止，等待用户验收。

## 阶段 1：获取固定依赖

- 固定 CherryUSB v1.6.1、完整 commit、归档 SHA256，CMake 获取并校验；不跟随 master。
- 沿用 FreeRTOS 管理方式，第三方源码不入库；原样保留上游目录、示例和许可证。
- 最小构建只选择 Device Core、CDC ACM、FSDEV，不编译上游示例入口、Host、其他类或 HAL 胶水。
- 单总线、Full Speed、4 字节对齐，关闭线程化 EP0 和中断打印，不引入 HAL PCD 或动态 OSAL 任务。
- 构建归档目标与固件链接分开；仅归档阶段不要求低层钩子已经实现，后续适配就绪再启用固件链接。
- 核验：首次获取与哈希验证、错误版本检查、完整源码下离线配置构建及编译清单。
- 规范例外：上游 FSDEV 在 middleware 下直接访问寄存器，保留上游结构，不复制或搬迁。
- 交付后停止。

## 阶段 2：系统时钟、IO 与重新枚举

- System_Clock_Init 负责 HSI48、USB 时钟源和 CRS，保持 170 MHz 主 PLL；不等待主机 SOF 才完成启动。
- BSP 持有引脚、IRQ 和时序参数；usb_io 只操作引脚，usb_dc_port 管外设门控、复位及中断。
- 系统初始化完成后，在启动任务创建通知目标，再禁用 USB IRQ，保持 USB 复位 200 ms，释放复位并清挂起中断，PA11/PA12 输出低 300 ms，再恢复 USB 所需引脚状态。时长参考 foc_drive_software，不宣称为规范规定值。
- 长等待使用任务阻塞时间服务，不用 DWT 忙等；完成状态、描述符及端点准备后再允许连接和有效中断处理。USB 优先级为 6，满足当前 FreeRTOS 门限。
- 本阶段实现硬件准备及控制器适配，不提前启动未就绪的 CDC 功能；完整连接放阶段 3。
- 核验：时钟源、CRS、PMA 参数、中断向量和启用顺序；插线/不插线都能启动。持续插线复位在阶段 5 最终验收。
- 规范：初始化参考 CubeMX LL；硬件就绪等待保留超时，porting 不反向依赖 BSP。外设门控不同于系统时钟树配置。
- 交付后停止。

## 阶段 3：CDC 传输

- 单 CDC ACM、两个接口；Bulk IN 0x81、Bulk OUT 0x02、Interrupt IN 0x83，Bulk 最大包长 64 B。
- BSP 获取 UID 并传入稳定序列号；供电属性按实板填写。本地测试沿用上游测试 VID/PID，正式发布另定。
- RX/TX 各 64 B 静态缓冲；单任务使用，RX 待取时 NAK 背压，TX 完成前不覆盖；处理必要 ZLP。
- 短临界区协调共享状态和提交，不单靠 volatile；支持复位、取消配置、挂起和恢复。不以 DTR 触发复位或升级。
- 完成初始化、读写、状态查询和事件契约，未连接电脑不算启动故障。真正初始化失败记录 STARTUP | INTERNAL，保留 LED 故障指示。
- 核验：描述符、序列号、枚举和基本收发，缓冲生命周期、BUSY、ZLP及会话重置。
- 规范：统一 Sys_Status；不增加空 driver，不在 ISR 解析命令或打印。
- 交付后停止。

## 阶段 4：文本命令与 DFU

- app_usb_console 实现上述行协议；app_usb_task 负责等待与分块收发。静态初始栈 1024 B，优先级 2。
- ISR 只搬运必要数据、更新状态、通知任务；命令通过 BSP_Platform 获取 UID、复位或进入 DFU。
- 实现 EnterDFU 的平台薄接口和 G431 ROM 交接：USB 断开，中断/SysTick 清理、时钟/映射交接、设置主栈及跳转；依据 G431 文档确认地址和前提，不写选项字节。
- 最终 MSP 切换使用独立汇编段，不能继续依赖旧 C 栈。调用契约是任务上下文且电机/采样已停止；当前未启动控制业务，未来启用后调用方负责停机。
- 核验：主机解析/派发测试、响应完成或超时后的执行顺序、Debug/Release 及跳转审查。
- 规范例外：少量交接汇编用于保证栈切换正确，不放业务。
- 交付后停止。

## 阶段 5：联合验收

- 回显正文长度 1/63/64/65/127/128；CR、LF、CRLF、空行、未知行、超长恢复、分包和多行同包。
- 至少 10 分钟且累计至少 1 MiB 文本无差异；串口重开 20 次。人工 USB 拔插项按用户明确要求取消，不作为本阶段完成条件。
- USB 线一直插着，reboot 20 次；每次重新枚举后主机关闭旧句柄、按稳定序列号重新找端口，继续通信。
- uid、USB 序列号一致。dfu 后 CDC 消失，ST DFU 可识别，CubeProgrammer 下载校验、恢复启动后 CDC/命令正常；另验证手动 DFU。
- 参数页先备份，禁止整片擦除，下载后逐字节比较；不扩展到自动回滚或断电恢复保证。
- 无越界、断言、忙等；任务栈最低余量至少 256 B，不足则调整并复验；LED/编码器无回归。
- 汇总 Flash/RAM 和实际测试结果，不将现阶段结果当作未来 FOC 实时性证明。
- 交付后停止，等待最终验收。

## 最终目录与阶段归属

仅列有变更的部分；[+] 新增，[M] 修改，[下载] 不入库；S0–S5 为首次实施阶段。后续阶段只完善对应文件，不提前实现。

```text
foc_gb4310_ll/
├─ [M/S1] .gitignore                         # 忽略下载的库，不改变测试忽略约定
├─ [M/S1–S4] CMakeLists.txt                  # 接入库及项目适配，不堆放依赖细节
├─ cmake/
│  ├─ [+/S1] cherryusb.cmake                 # 固定下载、校验、库源码清单
│  └─ [+/S1] cherryusb.sha256                # 编译输入及许可证的上游哈希清单
├─ Docs/project_memory/
│  ├─ [M/S0] README.md                       # 文档索引
│  ├─ [M/S5] 02_session_context.md           # 最终能力、约束和未完成边界
│  ├─ [M/S5] 04_architecture.md              # 已落地的分层、时钟和启动顺序
│  ├─ [M/S5] 05_platform_rtos.md             # 已落地的平台接口与任务配置
│  ├─ [+/S0] 08_cherryusb_plan.md            # 审核计划、阶段状态和目录职责
│  └─ [+/S0] 09_usb_validation.md            # 基线、核验步骤与真实结果，逐阶段更新
├─ project/
│  ├─ application/                          # 文本业务和调度，不直接访问硬件
│  │  ├─ [M/S2–S4] main.c                   # 启动编排、任务创建和失败处理
│  │  ├─ task/
│  │  │  ├─ [M/S4] app_task.h               # USB 任务入口声明
│  │  │  └─ [+/S4] app_usb_task.c            # 事件等待、收发调度，不处理寄存器
│  │  └─ usb/
│  │     ├─ [+/S4] app_usb_console.c         # 行解析、回显和三条命令，不是通用 shell
│  │     └─ [+/S4] app_usb_console.h         # 应用解析与派发接口
│  ├─ common/
│  │  └─ [+/S3] usb_cdc_types.h              # BSP/porting 共用事件契约，仅定义类型
│  ├─ bsp/                                  # 板级参数与能力绑定
│  │  ├─ [M/S2] bsp_board_config.h           # 引脚、IRQ、时序参数
│  │  ├─ platform/
│  │  │  ├─ [M/S2,S4] bsp_platform.c         # S2 更新时钟契约，S4 新增 DFU 薄接口
│  │  │  └─ [M/S2,S4] bsp_platform.h         # 系统时钟及平台 DFU 调用契约
│  │  └─ usb/
│  │     ├─ [+/S2] bsp_usb_cdc.c             # 编排时序，S3 绑定 UID 和传输
│  │     └─ [+/S2] bsp_usb_cdc.h             # 应用可见契约，不暴露库/LL/RTOS 类型
│  ├─ porting/                              # 平台及第三方适配，不解析命令
│  │  ├─ stm32g4/
│  │  │  ├─ [M/S2] system_clock.c            # 主频、HSI48、USB 时钟源、CRS
│  │  │  ├─ [M/S2] system_clock.h            # 系统时钟初始化契约
│  │  │  ├─ [+/S4] system_bootloader.c       # G431 ROM Bootloader 硬件交接
│  │  │  ├─ [+/S4] system_bootloader.h       # 系统内部交接入口，不涉及文本
│  │  │  └─ [+/S4] system_bootloader_jump.S  # 最终主栈切换与跳转
│  │  └─ cherryusb/
│  │     ├─ [+/S1] usb_config.h              # 库配置，S2 核对 PMA/IRQ 所需参数
│  │     ├─ [+/S2] usb_io.c                  # 引脚拉低、恢复，不配置时钟树
│  │     ├─ [+/S2] usb_io.h                  # 下传引脚配置及 IO 接口
│  │     ├─ [+/S2] usb_dc_port.c             # 控制器门控/复位、中断及低层钩子
│  │     ├─ [+/S2] usb_dc_port.h             # BSP 组织复位与 IRQ 的内部契约，不混入 GPIO 头
│  │     ├─ [+/S3] usb_cdc.c                 # 端点、静态缓冲及传输状态
│  │     ├─ [+/S3] usb_cdc.h                 # 供 BSP 绑定的传输接口
│  │     ├─ [+/S3] usb_descriptors.c         # 描述符和传入序列号，不直接取 UID
│  │     └─ [+/S3] usb_descriptors.h         # 描述符参数与注册接口
│  └─ middleware/third_party/
│     ├─ [M/S1] README.md                    # 版本、哈希、来源和许可证
│     └─ [下载/S1] CherryUSB/                # 上游完整布局，不修改、不格式化
└─ tests/                                   # 本地保留，不入库
   ├─ [+/S5] usb_cdc_console.py              # 实板回显、重连和命令验收
   └─ [+/S4] test_usb_console.py             # 主机解析和命令派发测试
```

不新增 system_usb.c/.h，也不使用 usb_io_stm32.c/.h。后续阶段若发现需要调整公共边界或验收行为，先更新具体差异交用户审核，不能静默扩大范围。

阶段 2 目录细化：新增 usb_dc_port.h，供 BSP 调用控制器准备与 IRQ 接口；避免把控制器声明放进只管引脚的 usb_io.h。控制器中断引用 FSDEV，库从本阶段开始实际链接，但不启动设备栈。增加主机模拟测试验证此边界，未越入阶段 3。

阶段 3 目录细化：新增 common/usb_cdc_types.h 统一回调/事件类型，公共 BSP 头不依赖 porting 或第三方头，也不复制事件枚举。main 暂用 appUsbEvents 记录累计事件供调试，不新增任务或文本业务；阶段 4 替换为任务通知。接收/发送接口实际在项目实现和主机测试中可用，固件无调用方时可被链接器回收，阶段 4 业务接入后自然保留。

用户确认硬件可由 USB 供电且 FOC 有外部 24 V；电源 OR-ing/开关拓扑、VBUS 感知及最大实际取电仍未知。当前 100 mA 总线供电描述符只用于本地验证，不能视为已测功耗。软件不增加供电关闭功能；USB 断开上拉不等于切断 VBUS。

阶段 3 用户补充要求：USB 标识和字符串先使用 ST 默认值，覆盖原 FFFF/FFFF 示例参数。采用 VID/PID=0483/5740、制造商 STMicroelectronics、产品 STM32 Virtual ComPort、配置 CDC Config、接口 CDC Interface。保留 CherryUSB 的 IAD/端点布局和完整 24 位 UID 序列号，不复制 ST 的协议栈或 12 位序列号算法。原标识版本已在实板重插后枚举为 COM10；新标识版本已编译测试，尚未烧录确认。

## 编码与后续替换

UTF-8、根 clang-format、中文 Doxygen、作者 kaouduo；第三方代码除外。固定启动契约下不加重复空指针、上下文或初始化检查，保留硬件超时和资源竞争处理。后续正式 shell 只替换 application 文本入口，复用 BSP USB 和平台接口。

资料入口：[CherryUSB v1.6.1](https://github.com/cherry-embedded/CherryUSB/releases/tag/v1.6.1)、[ST AN2606](https://www.st.com/resource/en/application_note/an2606-stm32microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)、[ST AN3156](https://www.st.com/resource/en/application_note/an3156-usb-dfu-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf)。阶段 0 仅记录后续依据，具体 G431 ROM 交接与勘误在实现前核对。

## 阶段 4 实际交付与目录差异

用户明确 main 可调用 porting，其他应用任务不能直接调用。删除 BSP_Platform_Init，main 直接顺序调用 System_Clock_Init、DWT_Init，失败仍锁存启动故障并断言；Board_IO_Init 保持原顺序。此项符合用户明确的既有规范。

USB 任务在初始化设备栈前创建，先等待 START 通知，保证 IRQ 通知目标有效；初始化失败删除任务。任务静态栈 1024 B、优先级 2。BSP_USB_CDC 增加 Deinit 和 SessionId 薄接口：前者供 DFU 断开，后者传递总线会话代次，在短临界区内检查代次和读写/接受文本，避免旧分包或响应跨复位重放。已接受的 reboot/dfu 不因断开取消。

平台 DFU 先停止 CDC 并任务延时 300 ms，再由 system_bootloader 校验 ROM 向量、关闭中断/SysTick、切回 HSI，随后清理外设（含 PWR）、PLL、HSI48/CRS，重映射 ROM，最后通过独立汇编清 BASEPRI/FAULTMASK/CONTROL 并切换 MSP 跳转。先降频再复位 PWR，避免 170 MHz 下撤销 Boost。不写 Flash、链接布局或选项字节。

当前库的 FSDEV 未上报 suspend/resume；项目 IRQ 适配层在上游处理后读取 FSUSP，交由 CDC 同步挂起状态，不修改第三方源码。主机测试覆盖该适配入口，物理挂起/恢复仍需上板验证。

以下仅列本阶段变化，沿用完整目录树的其他文件：

```text
foc_gb4310_ll/
├─ [M] CMakeLists.txt                       # 添加应用文本、USB 任务及 ROM 交接编译输入
├─ Docs/project_memory/
│  ├─ [M] 02_session_context.md             # 当前进度与 main 的调用边界
│  ├─ [M] 04_architecture.md                # main/业务任务分层和新启动顺序
│  ├─ [M] 05_platform_rtos.md               # 移除组合初始化，增加 USB 任务与 DFU 契约
│  ├─ [M] 08_cherryusb_plan.md              # 阶段状态及目录变化
│  └─ [M] 09_usb_validation.md              # 本阶段核验及硬件待验证项
├─ project/application/
│  ├─ [M] main.c                           # 直接初始化时钟/DWT；创建 USB 任务并通知启动
│  ├─ task/
│  │  ├─ [M] app_task.h                    # USB 任务入口与启动通知位
│  │  └─ [+] app_usb_task.c                 # 分块收发、会话清理、响应后执行平台动作
│  └─ usb/
│     ├─ [+] app_usb_console.c              # CR/LF/CRLF 行解析、回显及三条命令
│     └─ [+] app_usb_console.h              # 静态解析状态与应用内部接口
├─ project/bsp/
│  ├─ platform/
│  │  ├─ [M] bsp_platform.c                 # 删除 Init，增加 CDC 断开/等待/ROM 交接编排
│  │  └─ [M] bsp_platform.h                 # EnterDFU 调用契约，保留时间/UID/Reset 能力
│  └─ usb/
│     ├─ [M] bsp_usb_cdc.c                  # 增加 Deinit、SessionId 薄接口
│     └─ [M] bsp_usb_cdc.h                  # 应用可见生命周期与会话代次
├─ project/porting/
│  ├─ stm32g4/
│  │  ├─ [+] system_bootloader.c            # G431 ROM 硬件交接与硬件异常复位
│  │  ├─ [+] system_bootloader.h            # porting 内部交接契约
│  │  └─ [+] system_bootloader_jump.S       # 最终主栈切换及不返回跳转
│  └─ cherryusb/
│     ├─ [M] usb_dc_port.c                  # IRQ 尾部同步 FSDEV 挂起状态
│     ├─ [M] usb_cdc.c                      # 会话代次、反初始化及挂起状态同步
│     └─ [M] usb_cdc.h                      # 供 BSP/控制器适配使用的内部接口
└─ tests/                                  # 本地忽略，不入库
   ├─ [+] test_usb_console.py               # 编译并执行真实 C 实现的主机核验
   ├─ [+] usb_console_host.c                # 行边界、结束符、命令与超长恢复
   ├─ [+] usb_task_host.c                   # 分包、背压、响应完成/超时及会话取消
   ├─ [+] usb_boot_host.c                   # ROM 向量、清理顺序及超时复位
   └─ [+] usb_platform_host.c               # CDC 断开→300 ms 任务延时→ROM 交接
```

测试使用真实 C 解析/任务/交接实现，细化原计划的本地测试输入，不维护第二份 Python 解析器。阶段 4 已完成，当前按用户授权进入阶段 5 联合验收。

## 阶段 5 实际目录变化

```text
foc_gb4310_ll/
├─ Docs/project_memory/
│  ├─ [M] 02_session_context.md              # 当前验收状态与约束
│  ├─ [M] 03_freertos_clion_debug.md          # J-Link 持久会话的正常使用配置
│  ├─ [M] 04_architecture.md                 # 最终资源占用
│  ├─ [M] 08_cherryusb_plan.md               # 阶段状态、人工拔插豁免及目录差异
│  └─ [M] 09_usb_validation.md               # 实板计数、原始日志位置、限制与整改证据
├─ project/application/task/
│  └─ [M] app_usb_task.c                     # 连续收包时先检查缓冲，再进入阻塞等待
├─ tests/                                   # 本地忽略，不入库
│  ├─ [+] usb_stage5_serial.ps1              # Windows 串口边界/重开/复位/长测
│  └─ [M] usb_task_host.c                    # RX 通知被发送等待消费后的待取包回归
└─ build/usb_stage5/                         # 本地忽略，不入库
   └─ [+] 测试结果、参数页备份及下载日志       # 保留实际证据，不放生产代码
```

本机使用 PowerShell/.NET 串口测试，避免为一次本地验收引入 pySerial；功能覆盖原计划主机验收范围。联合测试发现的任务等待缺口在 application 修正，未改变 BSP/porting 公共边界、第三方源码或 Flash 布局。原始测试脚本和寄存器/下载日志不入库，文档记录可审阅的结果与局限。
