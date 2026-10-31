# USB 基线与验收记录

日期：2026-10-31（Asia/Taipei）。本记录只将实际执行过的检查标记通过。

## 阶段状态

| 阶段 | 状态 |
|---|---|
| 0 基线与文档 | 用户以“继续”验收；硬件未知项仍保留 |
| 1 固定依赖 | 用户以“继续”验收 |
| 2 系统时钟与 IO | 用户以“好的 请你继续”验收代码阶段，实板待验证项保留 |
| 3 CDC 传输 | 代码/主机协议核验及实板 CDC 枚举、收发通过 |
| 4 文本命令与 DFU | 用户已验收回显、uid/reboot、持续插线 DFU 下载/恢复闭环 |
| 5 联合验收 | 已完成本轮自动验收；人工拔插按用户要求取消，等待最终审核 |

## 阶段 0 开始时源码基线

- 当前工程：`E:/my-documents/research/stm32/my_gb4310_drive/foc_gb4310_ll`。
- 基线提交：`bc8caaa`，`refactor(clock): 按 CubeMX LL 风格整理系统时钟初始化`。
- 阶段开始时已有未提交修改：`Docs/project_memory/02_session_context.md`，为前轮 USB 选型及 CubeMX LL 风格记忆，本阶段保留未改。
- STM32G431CBU6，HSI16 → PLL M=4/N=85/R=2 → 170 MHz；当前没有启用 HSI48、USB 时钟源或 CRS。
- 当前尚未接入 USB 设备栈，USB 中断向量仍使用默认弱处理器。当前不具备 CDC、文本命令或软件 DFU 接口。
- Flash 链接起点 `0x08000000`，长度 126 KiB；参数区 `0x0801F800–0x0801FFFF`，2 KiB；RAM 32 KiB。
- SysTick 归 FreeRTOS；NVIC 系统调用门限 5。当前 PWM 和 ADC 文件只准备 IO，没有启动电机控制或同步采样业务。

## 构建与资源检查

实际执行 `cmake --build --preset REV6-Debug`、`cmake --build --preset REV6-Release`，均退出 0；Ninja 报告 no work to do，属于现有产物的增量构建核验，不是清理后全量重编译。

使用 ARM GNU 14.2 的 `arm-none-eabi-size` 读取两个 ELF：

| 配置 | text | data | bss | Flash（text+data） | RAM（data+bss） |
|---|---:|---:|---:|---:|---:|
| REV6-Debug | 28124 B | 12 B | 24860 B | 28136 B / 21.81% | 24872 B / 75.90% |
| REV6-Release | 14940 B | 12 B | 24860 B | 14952 B / 11.59% | 24872 B / 75.90% |

Flash 比例按 126 KiB 链接区计算。RAM 数值包含 16 KiB FreeRTOS 堆和链接器堆栈预留，不代表运行时自由堆或任务栈余量。现有架构记忆中的 Debug 21.69% 是前次快照；本表是 bc8caaa 的重新核验值，本阶段不修改旧快照为已实现 USB 状态。

## 参考工程证据（不是本板实测）

| 参考文件 | 观察到的内容 |
|---|---|
| `../foc_drive_software/FOC.ioc` | PA11=USB_DM，PA12=USB_DP；HSI48 作为 USB 时钟源；USB IRQ 优先级 5 |
| 同一 `.ioc` | PB8-BOOT0 配为 FDCAN1_RX，不能据此认定 BOOT0 有可用按键或跳线 |
| `../foc_drive_software/project/porting/usb_cdc/usb_cdc_stm32.c` | PrepareEnumeration 禁 IRQ，USB 外设保持复位 200 ms，释放/清挂起，再将 PA11/PA12 输出低保持 300 ms |
| `../foc_drive_software/project/application/app_main.c` | 任务内 PrepareEnumeration 后启动 MX_USB_Device_Init |

新实现保留 200/300 ms 初值，显式恢复 USB 所需引脚状态；不能假定 CherryUSB 会重现 HAL 的引脚接管行为。参考工程的优先级不直接照抄，本计划选 6。

## 只读设备枚举

实际执行 `STM32_Programmer_CLI -l`，工具版本 2.23.0，退出 0：

- 未发现 ST DFU 设备。
- 未发现 ST-Link。
- 发现一个 J-Link；串口 COM9 为 SEGGER JLink CDC UART Port，不是目标 MCU 的 USB CDC。

没有连接调试会话，没有读取选项字节、复位、烧写、擦除或修改芯片。发现 J-Link 不代表其已连接本项目目标板。

## 硬件及启动待确认项

| 项目 | 当前证据/状态 | 后续核验方式 |
|---|---|---|
| 实板 PA11/PA12 与连接器 | 只有旧 `.ioc`，未确认实板 | 核对 REV6 电路和实际接线 |
| 供电、VBUS、连接器及外部上拉 | 未确认 | 核对实板，决定描述符和断开方式 |
| BOOT0 接线与启动选项字节 | 未读；不能推定默认值 | 确认调试目标后只读记录，再核对 G431 启动条件 |
| 手动 ROM DFU | 未试验 | 硬件启动进入后由 CubeProgrammer 枚举 |
| 软件 DFU 与交接 | 尚未实现 | 阶段 4 实现，阶段 5 执行 dfu 验证 |
| 持续插线普通复位 | 尚未实现 USB | 阶段 5 连续 reboot 20 次 |
| 参数页内容与升级保留 | 未读取，不进行任何擦写 | 下载前备份、限定擦除范围、下载后比对 |

阶段 0 文档与构建部分通过；硬件项仍待确认，不宣称整板 USB 或 DFU 已可用。后续进入受这些条件影响的实板步骤前必须完成对应确认。

## 后续验收记录模板

每阶段记录日期、源码状态、改动文件、执行命令/工具、结果、规范例外、未验证项及用户验收结论。完整用例见 [计划](08_cherryusb_plan.md)。当前尚无 CDC 收发、枚举、命令、ROM DFU 或下载测试通过记录。

阶段 0 原始交付时未下载 CherryUSB；用户随后以“继续”允许进入阶段 1。未提交或推送。

## 阶段 1：依赖获取与构建

### 交付内容

- 新增 `cmake/cherryusb.cmake`、`cmake/cherryusb.sha256`、`project/porting/cherryusb/usb_config.h`。
- 修改根 CMakeLists、gitignore、第三方 README 及阶段文档；不修改 main、时钟、IRQ、BSP 或任何上游源码。
- 固定 v1.6.1，commit `c9625ffa773ad10b8824d1b5361bca2ccc1f3d1e`；通过 `git ls-remote` 核对 tag。
- 发布归档 SHA256：`04862e5ffcbacc5747bc1b0fa068d58be1e307761bd2076e78c6ceaeda9f028b`。
- CMake 首次缺目录时实际联网获取并校验归档；已有目录检查版本和 23 项源码/头文件/许可证哈希。
- 构建 `cherryusb_device` 静态库，尚未链接固件，不运行库中的弱低层初始化钩子。
- 配置为单总线、Full Speed、4 字节对齐、256 B 控制请求缓冲；不打印，保留断言，不启用 EP0 线程或 OSAL 任务，不添加链接段。PMA 访问值为 1，容量和硬件适配留到阶段 2。

### 实际执行结果

| 检查 | 结果 |
|---|---|
| 首次 `cmake --preset REV6-Debug` | CMake 下载并校验归档，配置通过 |
| `cmake --build --preset REV6-Debug` | 三个 CherryUSB 源文件实际编译并生成静态库，固件构建通过 |
| `cmake --preset REV6-Release -DFETCHCONTENT_FULLY_DISCONNECTED=ON` | 使用已获取源码，无需下载，配置通过 |
| `cmake --build --preset REV6-Release` | 三个源文件实际编译并生成 Release 静态库，通过 |
| 新目录 `build/cherryusb-offline`、Debug、`FETCHCONTENT_FULLY_DISCONNECTED=ON` | 独立配置及全部 40 步编译/链接通过；未通过操作系统断网，仅禁止 FetchContent 获取依赖 |
| compile_commands 与静态库成员 | 三种构建都仅含 usbd_core.c、usbd_cdc_acm.c、usb_dc_fsdev.c；库包含对应三个对象 |
| ELF 符号检查 | 不含 usbd_、usb_dc_、g_fsdev 符号，确认未链接 USB 栈 |
| 比较下载目录与发布归档 | 全部 860 个上游文件逐字节一致 |
| Git 忽略检查 | 下载库和本地测试产物均被忽略 |

离线独立构建命令：

```powershell
cmake -S . -B build/cherryusb-offline -G Ninja '-DCMAKE_TOOLCHAIN_FILE=E:/my-documents/research/stm32/my_gb4310_drive/foc_gb4310_ll/cmake/gcc-arm-none-eabi.cmake' -DCMAKE_BUILD_TYPE=Debug -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build/cherryusb-offline
```

离线校验夹具位于被忽略的 `tests/cherryusb_dependency/`，只修改测试副本，没有修改下载库：

| 夹具 | 结果 |
|---|---|
| 原始版本与完整 23 项输入 | 配置通过 |
| 版本头改为 v1.6.0 | 按版本错误拒绝 |
| 缺失 core/usbd_core.c | 按源码不完整拒绝 |
| CDC 源文件内容变化 | 按哈希不一致拒绝 |
| 离线且源码目录不存在 | 明确提示源码缺失并拒绝 |

各夹具保留 configure.log 和 results.json。原始检查中的路径分隔符判断及 PowerShell 工具链参数引号问题已修正后重新核验，不属于固件失败。

固件尺寸与阶段 0 一致：Debug Flash 28136 B，Release Flash 14952 B，RAM 均 24872 B。库还未链接，不能将此尺寸当作完整 USB 功能资源占用。

### 规范及待验证项

- 符合第三方不修改/不格式化/不入库规则，仅对第三方目标关闭未使用参数警告。
- 比原目录计划新增一份 `cmake/cherryusb.sha256`：为已有源码离线核验编译输入，避免只校验版本字符串；已同步计划目录树。
- 保留第三方 FSDEV 驱动在 middleware 下操作寄存器这一已说明例外。
- 未接入硬件、IRQ、CDC 实例、命令或 DFU，实板未知项沿用阶段 0。
- 用户阶段 1 验收：待审核。停止于此阶段，不进入阶段 2，不提交或推送。

用户随后以“继续”验收阶段 1，进入阶段 2。

## 阶段 2：系统时钟与启动准备

### 实现与边界

- `system_clock.c/.h` 增加 HSI48 就绪超时、USB 时钟源选择、CRS SOF 自动校准及源读回检查；主 PLL 仍为 170 MHz，不等待 SOF 锁定。CRS 参数：分频 1、上升沿、reload 47999、error limit 34、trim 32，启用自动修调和误差计数，不开启 CRS IRQ。
- `bsp_platform.c/.h` 仅同步初始化契约，不增加 USB 操作能力表。
- `bsp_board_config.h` 固定 PA11/PA12、GPIOA、IRQ 优先级 6、200/300 ms；实际实板条件仍待确认。
- 新增 `bsp/usb/bsp_usb_cdc.c/.h`：BSP_USB_CDC_Prepare 组织控制器复位、阻塞等待、引脚拉低与释放。main 的启动任务在编码器和 Flash 初始化后执行，再发布状态、创建 LED 任务；不新增 USB 任务。
- 新增 `porting/cherryusb/usb_io.c/.h`：GPIO 时钟及引脚输出低/无上下拉输入恢复，不设置 USB AF 或时钟树。
- 新增 `usb_dc_port.c/.h`：关闭 LP/HP/WakeUp IRQ、USB 时钟门控和复位、清挂起；v1.6.1 的无参数强钩子负责退出掉电、10 us DWT 短等待及反初始化断开。USB_LP_IRQHandler 转交 USBD_IRQHandler(0)，只有将来设备栈准备完成后才调用 EnableIRQ。
- 复位释放后 BCDR=0，CNTR=FRES|PDWN，保持断开；200/300 ms 为任务阻塞等待，未采用 500 ms 忙等。
- `usb_config.h` 显式配置 PMA_ACCESS=1、包内存 1024 B。未修改任何第三方文件。
- 当前链接 FSDEV 与被中断引用的设备核心；不调用 usbd_initialize、不启用 USB IRQ，电脑此阶段不会识别 CDC。尚无描述符、CDC 收发和文本命令。

### 硬件依据

[G431 数据手册 DS12589](https://www.st.com/resource/en/datasheet/stm32g431cb.pdf) 3.34 明确 USB 包内存最高 1 KiB，支持 HSI48 通过 SOF 自动校准；引脚表将 USB_DM/DP 列为 PA11/PA12 的附加功能而非 GPIO AF。

无上下拉输入释放方式同时参考 [TinyUSB STM32G4 板级初始化](https://github.com/hathach/tinyusb/blob/master/hw/bsp/stm32g4/family.c)，不是把其他型号 AF 编号套用到 G431。PMA 步长 1 来自项目已固定的 STM32CubeG4 LL 头文件。G431 头文件不定义 PWR_CR2_USV，因此不复制其他型号的 VDDUSB 开关调用。

### 实际执行结果

| 检查 | 结果 |
|---|---|
| Debug/Release 编译链接 | 通过，修正头文件依赖后无新增编译警告 |
| 主机模拟 `tests/usb_stage2_host.c` | 通过：170 MHz/HSI48/CRS 参数、HSI48 不就绪超时、源读回不符返回 IO |
| 同一主机模拟 | 通过：复位 → 200 ms → 释放 → 数据线低 → 300 ms → 输入恢复；保持 IRQ 关闭，预置输出低，无关引脚不变 |
| 同一主机模拟 | 通过：低层 PHY 准备不启 IRQ、显式开 IRQ、LP 中断转发、反初始化关闭 IRQ/上拉 |
| 原有 `tests/startup_host.c` 更新并运行 | 通过：调度前不执行准备，资源初始化后准备，成功/失败状态与 LED 创建顺序保持 |
| ELF 向量 | 两种配置 USB LP 为强符号；Debug 向量 0x08003D0D，Release 0x08001A9D，均等于实际入口地址加 Thumb 位 |
| 链接边界 | ELF 不含 usbd_initialize 或 USB_DC_EnableIRQ，启动路径没有开启 USB 或调用设备栈 |
| 分层检查 | USB porting 无 BSP 头依赖；应用只增加 BSP USB 头 |

本地主机模拟的编译执行方式：

```powershell
clang -std=c11 -Wall -Wextra -Werror -Itests/usb_stage2_mock -Iproject/common -Iproject/bsp/platform -Iproject/bsp/usb -Iproject/porting/cherryusb -Iproject/porting/stm32g4 tests/usb_stage2_host.c project/porting/stm32g4/system_clock.c project/bsp/usb/bsp_usb_cdc.c project/porting/cherryusb/usb_io.c project/porting/cherryusb/usb_dc_port.c -o build/usb_stage2_host.exe
./build/usb_stage2_host.exe
```

测试包含实际项目实现，替身不代替实板时钟精度、USB 电气时序或控制器验证。测试及替身保留在忽略的 tests/。

| 配置 | Flash | RAM | 相对阶段 1 |
|---|---:|---:|---|
| Debug | 43604 B / 33.80% | 26048 B / 79.49% | Flash +15468 B，RAM +1176 B |
| Release | 20852 B / 16.16% | 26048 B / 79.49% | Flash +5900 B，RAM +1176 B |

增加包含 FSDEV/设备核心的中断路径和 USB 硬件准备代码，不能将此占用当成最终完整 CDC 的尺寸。

### 规范、变更说明与待实测

- 遵守 LL 风格系统时钟、板级配置向下传递、GPIO/控制器分离、统一调用契约；不添加重复初始化或上下文防御分支。
- 新增 usb_dc_port.h 是职责明确的内部声明，不把控制器接口放入 GPIO 头；已补充计划树。BSP platform 实现只更新注释，未扩展行为。
- 当前引脚与时序配置采用参考工程作为初值，不代表实板接线已确认；本轮没有调试器连接、下载或硬件运行。
- 实板待测：HSI48/CRS 寄存器、引脚释放、200/300 ms 电气时序、未插线启动。重新枚举和 CDC 必须阶段 3 接通设备栈后才能验证，完整常连 reboot 在阶段 5 验收。
- 用户阶段 2 验收：待审核。停止，不进入阶段 3，不提交或推送。

用户随后以“好的 请你继续”允许进入阶段 3。

## 阶段 3：CDC 描述符与静态传输

### 实现与边界

- 新增 `usb_descriptors.c/.h`：单 CDC ACM 两接口，EP0 64 B、Bulk IN 0x81/OUT 0x02 各 64 B、通知 IN 0x83。Full Speed 不提供 device qualifier/other-speed 描述符。
- BSP 经现有 BSP_Platform_UniqueId 获取三字 UID，按 words[0] 到 words[2]、每字高位到低位生成 24 位大写十六进制序列号；描述符复制保存，不依赖启动任务的临时数组。
- 板级产品名 GB4310 REV6 CDC，制造商 kaouduo，测试 VID/PID=FFFF/FFFF。正式发布需有效标识。
- 用户确认硬件提供 USB 供电，FOC 有外部 24 V。尚未确认 MCU 电源路径、是否有软件控制开关及最大实际取电；暂采用总线供电/100 mA 测试声明，不声称是测量值，不增加电源切断功能。
- 新增 `usb_cdc.c/.h`：静态 RX/TX 各 64 B，复制发送、单任务非阻塞读写、部分包读取、NAK 背压、TX 必要 ZLP、配置/复位/挂起/恢复状态与错误处理。传输提交失败置 IO 状态，等待主机重新配置或复位，不忙等。
- SET/GET_LINE_CODING 保存和读回主机配置，默认 115200/8N1，不改变 USB 速率；控制线和 break 请求由类正常应答，不用 DTR/RTS/break 触发复位。
- 新增 `common/usb_cdc_types.h` 共享事件与回调类型；BSP 头不包含 porting、LL、第三方或 RTOS 头。
- USB_CDC_AfterIRQ 补齐上游未通知的 SET_CONFIGURATION(0)：关闭数据/通知端点，清除旧收发状态；不修改上游。
- USB 事件回调仅在 IRQ 通知。初始化/反初始化任务事件不转发到 IRQ 回调；任务读写错误直接通过返回码报告。
- main 改为 BSP_USB_CDC_Init，内部包含唯一一次重新枚举准备，注册完整状态后才开 LP IRQ；USB 失败锁存 STARTUP|INTERNAL，首次失败码优先于后续错误。无电脑连接不算初始化失败。
- 当前 application 只通过 appUsbEvents 累计 IRQ 事件用于调试器观察，不取走 RX、不回显、不解析命令；阶段 4 才接任务。未使用的 BSP 收发入口可被链接器回收，测试中通过真实调用验证其实现。

### 实际执行与结果

本地主机测试 `tests/usb_cdc_host.c` 编译实际 BSP、项目传输/描述符、上游 usbd_core.c 和 usbd_cdc_acm.c，仅替换控制器、时间/UID、临界区与硬件操作。不是只用假的配置事件替代枚举流程。

| 用例 | 结果 |
|---|---|
| 原始 GET_DESCRIPTOR 请求 | Device、Configuration、UID String 长度/内容、接口数、三个端点、64 B Bulk 包长均通过 |
| Full Speed qualifier 请求 | STALL，通过 |
| 初始化顺序 | 200/300 ms 准备、UID 传入、设备初始化后才开 IRQ，通过 |
| 模拟初始化失败独立进程 | 返回 IO、不启 IRQ、任务初始化期间不调用 IRQ 回调，通过 |
| SET/GET_LINE_CODING | 经真实 EP0 OUT 数据阶段写入并读取 9600/7位/奇校验/2停止位，通过 |
| 控制线状态为 0 | 正常应答且 CDC 仍可传输，通过 |
| RX | 6 B 输入后不重挂接；先取 2 B 再取 4 B 后重挂接；空包重挂接；无数据 BUSY，通过 |
| TX | 内部复制，调用者随后改写不影响发送；BUSY、短包、64 B 后 ZLP 完成才 TX_DONE，通过 |
| 输入范围 | Write 0/65 B、Read 容量 0 返回 PARAM，通过 |
| 会话 | suspend/resume、带在途数据的 SET_CONFIGURATION(0)、reset、重新配置无旧数据，通过 |
| 错误恢复 | 普通发送、ZLP、读取重挂接、首次挂接故障返回/通知正确；重新配置恢复，通过 |
| 原有启动测试 | 更新 USB 初始化模拟，新增 USB 初始化失败场景，状态/灯效/首次失败码检查通过 |
| 阶段 2 模拟回归 | 仅补充 CDC 依赖替身，原时钟/复位/GPIO/IRQ 测试仍通过 |
| Debug/Release | 编译链接通过，原始源码哈希未改 |
| 公共头与 ELF | C++17 独立包含检查通过；两种构建的 LP 向量、低层强钩子、line coding 强实现及设备初始化符号正确 |

补充用例：完整 64 B RX、挂起期间保留未取数据和在途发送、恢复后继续完成均通过。主机模拟不代表物理断电后的数据可保留。

本地执行方式（PowerShell）：

```powershell
clang -std=c11 -Wall -Wextra -Wno-unused-parameter -include tests/usb_cdc_mock/compiler.h -Itests/usb_cdc_mock -Iproject/common -Iproject/bsp/platform -Iproject/bsp/usb -Iproject/porting/cherryusb -Iproject/middleware/third_party/CherryUSB/common -Iproject/middleware/third_party/CherryUSB/core -Iproject/middleware/third_party/CherryUSB/class/cdc tests/usb_cdc_host.c project/bsp/usb/bsp_usb_cdc.c project/porting/cherryusb/usb_cdc.c project/porting/cherryusb/usb_descriptors.c project/middleware/third_party/CherryUSB/core/usbd_core.c project/middleware/third_party/CherryUSB/class/cdc/usbd_cdc_acm.c -o build/usb_cdc_host.exe
./build/usb_cdc_host.exe
./build/usb_cdc_host.exe fail-init
```

测试副本/替身不修改上游；compiler.h 仅给 Windows 主机测试补充 ARM 编译器打包/弱符号宏。任务读写与 USB IRQ 使用当前 FreeRTOS 临界区门限协调；ISR 内不调用任务阻塞 API。

| 配置 | Flash | RAM | 相对阶段 2 |
|---|---:|---:|---|
| Debug | 49352 B / 38.25% | 26360 B / 80.44% | Flash +5748 B，RAM +312 B |
| Release | 23744 B / 18.40% | 26360 B / 80.44% | Flash +2892 B，RAM +312 B |

当前尺寸未包含阶段 4 的文本缓冲、任务栈和实际收发调用保留成本，不是最终尺寸。

### 规范、待上板与验收操作

- 分层符合：应用只调 BSP；UID 只经 BSP_Platform 获取；common 仅放共享类型；无 HAL、动态分配或空 driver。
- 目录细化新增 common/usb_cdc_types.h，避免公共头依赖 porting 或两层复制事件定义，已同步计划。
- 回调和静态缓冲地址不指向启动任务栈。初始化只按固定顺序一次，不增加重复初始化保护、空指针或上下文防御。
- 现有静态库上游源码保持不变；类头依赖 core 的宏和类型，以独立 include 分组保留正确顺序，避免格式化排序破坏依赖。
- 实板尚未烧写或枚举；不能将主机测试当作 Windows COM 驱动、48 MHz 精度或物理连线验证。
- 上板可先观察 appStartupResult=SYS_OK 和 appUsbEvents 状态位、电脑是否出现产品 CDC、序列号是否稳定；本阶段发送数据不会自动回显。连接失败时先核对阶段 0 的电源/线路和阶段 2 时序。
- 协议栈没有 VBUS 检测输入，IsConfigured 只是协议可传输状态，不是电源检测；无 VBUS 感知时物理拔线通常先表现为 suspend，不能保证即时 DISCONNECTED 通知。
- 100 mA、电源切换、防止外部电源向 VBUS 回灌等硬件条件尚未验证；配置描述符或关闭 USB 外设不会切断 USB 5 V。
- 用户阶段 3 验收：待审核。停止，不进入阶段 4；没有新增回显、命令、DFU 或烧写动作，未提交/推送。

### 阶段 3 实板枚举反馈与 ST 标识调整

用户最初反馈 Windows 代码 43、“设备描述符请求失败”。本轮使用已连接 J-Link 只读连接运行中的 G431：

- 读取 DBGMCU_IDCODE=0x20036468、HSI48 就绪、USB LP IRQ 已使能、优先级 6；未见 HardFault/FreeRTOS 断言，CPU 正在运行。
- `verifybin` 对目标 Flash 的 49352 B 与修改标识前的 Debug bin 校验成功，没有下载、擦除、复位或修改内存。
- 初次观察 USB 处于 suspend，EP0/FSDEV 状态为零，尚未处理总线复位；不能据此认定描述符内容错误。
- 用户拔插目标板 USB 后，读取到设备已配置，Windows 当前设备列表显示 USB 串行设备 COM10 和 VID_FFFF/PID_FFFF 的 Composite Device，序列号为 `00210040484E500220333031`。用户截图也确认硬件 ID 和产品名 GB4310 REV6 CDC。

因此记录为“旧标识固件重插后 CDC 枚举通过”，不是已证明持续插线软件复位可靠，也不能确定先前失败的唯一原因。仍保留阶段 5 的常连 reboot 验收。

用户随后要求先使用 ST 官方描述标识。本次改动：

| 参数 | 新值 |
|---|---|
| VID / PID | 0x0483 / 0x5740 |
| 制造商 | STMicroelectronics |
| 产品 | STM32 Virtual ComPort |
| 配置字符串 | CDC Config，索引 4 |
| 通信/数据接口字符串 | CDC Interface，索引 5 |
| UID 序列号 | 保留 BSP_Platform 提供的完整 24 位格式 |

标识参考现有 CubeMX 工程 `../foc_drive_software/USB_Device/App/usbd_desc.c`；[ST CubeG4 CDC 官方示例](https://github.com/STMicroelectronics/STM32CubeG4/blob/master/Projects/STM32G474E-EVAL/Applications/USB_Device/CDC_Standalone/USB_Device/App/usbd_desc.c) 同样使用 0483/5740 和 STMicroelectronics，产品名有 in FS Mode 版本差异。只替换标识与字符串，保留已跑通的 CherryUSB IAD/端点结构和版本字段，不把整份 ST 中间件描述符逐字节移入。

Debug/Release 构建与真实核心主机测试通过，增加了 VID/PID 及四项字符串的 EP0 请求检查。最新尺寸：Debug Flash 49416 B / 38.30%，Release 23796 B / 18.44%，RAM 均 26368 B / 80.47%。

新版本未烧录，Windows 截图的 FFFF/FFFF 是已枚举旧版本，不应解释为新源码仍为 FFFF。用户重新烧录最新固件并拔插后，应检查硬件 ID 中的 VID_0483/PID_5740 和总线产品描述 STM32 Virtual ComPort；Windows 可重新分配 COM 号，界面友好名称由驱动决定。

保持阶段 3，未进入阶段 4，未提交或推送。

### USB 初始化顺序

回调及描述符注册完成后，在 usb_dc_low_level_init 准备 PHY 并开启 LP NVIC，再由上游 FSDEV 配置控制器中断源并最后打开 D+ 上拉。IRQ 生命周期属于 porting，BSP 不再后置调用 USB_DC_EnableIRQ。主机模拟与 Debug/Release 构建通过。

## 阶段 4：启动入口整理、文本命令及 ROM DFU 交接

用户授权实施阶段 4。以下为阶段 4 初次交付时的记录，后续实板结果及阶段 5 状态见文末。

### 变更及边界

- `main.c` 直接调用 System_Clock_Init、DWT_Init，删除 BSP_Platform_Init 的声明与实现；平台服务实例、UID/Reset 能力保留。用户明确 main 可调用 porting，其他任务不能直接调用，已同步架构记忆。
- 新增 app_usb_console.c/.h：静态 128 B 正文、最大 130 B 响应；支持 CR/LF/CRLF、裸 CR 的 10 ms 判定、迟到配对 LF、精确命令匹配、原始结束符回显及超长丢弃恢复。不包含 USB/RTOS 类型，不是 shell。
- 新增 app_usb_task.c，扩展 app_task.h/main：栈 1024 B、优先级 2；先建通知目标，初始化成功再通知 START。单任务独占收发，响应按最多 64 B 分块；等待 TX_DONE（含 ZLP），普通响应按背压等待，reboot/dfu 响应完成或最多等待 200 ms 后执行。错误/断开不永久阻止已接受的命令。
- BSP/porting 的 CDC 增加 Deinit 和 SessionId。总线复位/取消配置/重新配置使会话代次改变；任务丢弃旧文本、未处理分包和响应，短临界区协调会话检查及收发/命令接受。挂起本身不建立新会话。
- FSDEV v1.6.1 没有向核心上报 suspend/resume；usb_dc_port 在上游 IRQ 处理后读取 FSUSP，usb_cdc 同步状态并通知任务。不修改或格式化第三方源码。
- BSP_Platform_EnterDFU 从任务中停止 CDC 并等待 300 ms，再调用 system_bootloader。porting 校验 ROM 向量，关闭 SysTick/NVIC、清挂起，切回 HSI 并停止 PLL 后再复位外设（避免 170 MHz 时撤销 PWR Boost），关闭 HSI48/CRS、恢复相关时钟选择和中断优先级、设置 VTOR/系统存储映射，独立汇编切换 MSP 并跳转。不修改应用起始地址、参数页或选项字节。

### 依据及限制

核对 [ST AN2606 Rev 70，G431/G441 第 52 节](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf)：G431 系统 ROM 起始 0x1FFF0000，28 KiB；支持 USB DFU，使用 HSI48/CRS 和 PA11/PA12。文档的 G431 USB 引脚表为 input no pull，内部上拉由 USB 控制器管理。ROM 也会探测其他下载接口，软件跳转不等同于可强制指定仅运行 DFU；实际识别须实板确认。

ROM 交接要求任务特权上下文、控制/采样已停止且参数已保存；当前尚未启动 FOC/ADC 控制业务。未来启用控制业务或看门狗后必须重新审查调用方停机与交接。当前时钟等待超时或 ROM 向量无效时软件复位，不伪报 DFU 成功。命令应答完成只代表 USB IN 事务完成，不代表主机应用已读到文本。

### 本地核验结果

| 项目 | 结果 |
|---|---|
| 真实 C 行解析 | 通过：1/63/64/65/127/128 B、129/300 B 超长恢复、CR/LF/CRLF、裸 CR/迟到 LF、空行、精确匹配、UID、毫秒回绕与会话清理 |
| 真实 USB 任务 + 模拟 BSP/RTOS | 通过：输入每次切分 1–64 B、多行同包、130 B 分三次输出、BUSY 背压、TX 完成后动作、未完成 200 ms 超时、计数回绕、断开后仍执行命令、旧会话余包取消 |
| 真实 ROM 交接 C + 模拟寄存器 | 通过：ROM 向量检查、SysTick/NVIC 清理、先切 HSI 再复位外设、向量/映射、HSI/时钟切换/PLL 退出超时复位；不能替代 ROM 执行实测 |
| 真实 BSP 平台 DFU 编排 | 通过：先 CDC Deinit，再任务 delay 300 ms，最后进入 ROM 交接 |
| 启动回归 | 通过：时钟和 DWT 失败不进入 IO；设备成功/失败状态、USB 任务通知目标和初始化失败删除路径 |
| 阶段 2 / 真实 CherryUSB 核心回归 | 通过：时钟、GPIO/复位/IRQ、描述符/UID/EP0、RX 背压、TX/ZLP、会话及初始化失败；挂起/恢复测试改为走新增项目同步入口 |
| C++ 公共头 | 通过：BSP 平台/USB 头独立包含；新增入口仍为 C 链接 |
| Debug/Release | 均编译链接成功；CMake 原始输入哈希校验通过 |
| 跳转汇编 | 两种 ELF 均含独立跳转符号；Debug 反汇编确认 BASEPRI/FAULTMASK/CONTROL 清零、MSP 切换、屏障、开中断和 BX，切栈后没有 C 栈访问 |
| 规范检查 | 自建 C/H 按 clang-format 格式化；git diff --check 通过；第三方源码不变，测试仍被忽略 |

本地测试入口：`python tests/test_usb_console.py`，使用 clang 编译真实 C 实现，执行 parser/task/boot/platform 四组核验；其他启动/阶段 2/CDC 回归继续使用原本地用例。测试目录未入库，不将其描述为仓库自带 CI。

| 配置 | Flash | RAM | 相对阶段 3 IRQ 整改版本 |
|---|---:|---:|---|
| Debug | 54752 B / 42.44% | 27904 B / 85.16% | Flash +5352 B，RAM +1536 B |
| Release | 26816 B / 20.78% | 27904 B / 85.16% | Flash +3000 B，RAM +1536 B |

静态 RAM 包含任务栈和 RTOS 堆；剩余 4864 B 不能当作任务运行栈余量。编译器栈报告不是实测高水位，阶段 5 仍需确认 USB 任务至少 256 B 余量。

### 用户阶段 4 验收

可先用用户已验证可工作的 CubeProgrammer 下载最新 `build/REV6-Debug/foc_gb4310_ll.hex` 并启动，不做整片擦除。串口终端关闭本地回显后，发送 `hello` 加行尾应返回原文本；`uid` 应返回 `UID=00210040484E500220333031` 加 CRLF（本板 UID），与 USB 序列号一致。其他板按实际 UID 返回。

`reboot` 应先尝试返回 OK reboot，再普通复位；`dfu` 应先尝试返回 OK dfu，然后 CDC 断开并进入 ST ROM。实际回显、常连 reboot/重新枚举、ROM DFU 驱动识别、DFU 下载恢复、参数页保留和 10 分钟长测均未实测，继续列为硬件待验收项；不因构建/主机测试成功标记通过。

架构符合：main 启动调用 porting、其他任务经 BSP；UID 由 BSP_Platform 提供；USB 系统时钟留在 system_clock；IO/控制器职责保留；无 shell、无动态文本分配、无新建 Bootloader。规范例外只有计划中明确的最终栈切换汇编。阶段 4 交付后停止，等待验收；不进入阶段 5，未提交或推送。

用户后续实板反馈：文本回显、uid、reboot 正常。此处记录为用户验证结果，尚无重复次数和长测数据；dfu 进入、USB 下载及恢复运行仍待验证。

### 阶段 4 DFU 实板故障定位与交接整改

用户发送 dfu 并收到 OK dfu，但 Windows 仍保留 COM11，CubeProgrammer 无 DFU 端口。只读检查得到 VTOR=0x1FFF0000、ROM 向量 MSP=0x20002CF0/入口=0x1FFF4B15，ROM ID=0xD4；之后短暂 halt 读取 PC=0x1FFF3BCE、CONTROL/BASEPRI/FAULTMASK/PRIMASK 均为 0，并立即 go 恢复。说明 ROM 已运行，没有回到应用，也不是中断屏蔽未解除。USB DADDR=0、EP0 尚未建立；旧 COM11 打开时报设备没有发挥作用，不能据设备管理器旧条目判定应用仍在运行。

用户确认仅由 USB 供电、未接外部 24 V，因此拔插会断电复位；这次重插不能作为保持 ROM 运行的重连验证。

在再次 dfu 且用户关闭串口后，执行目标寄存器诊断（没有下载/擦除 Flash 或写选项字节）：

- 仅关闭 D+ 上拉、拉低 PA11/12 后恢复，300 ms/1500 ms 两次均未恢复枚举；未将此假设直接写入固件。
- 将 PHY 明确置于 FRES|PDWN，保持 USB 控制器时钟有效，拉低数据线，再恢复引脚/PHY/IRQ/上拉后，主机识别 STM32 Bootloader，VID/PID=0483/DF11，ROM 序列号 20543071484E，USB DADDR=0x83、EP0=0x3220。随后按 300 ms 拉低重新执行完整 PHY 流程，也枚举成功。期间 MCU 未断电，ROM 向量保持不变。
- 证据将问题集中到 USB 硬件断开/恢复交接，尚未测量物理波形，不能把单个寄存器写入顺序认定为唯一原因。

整改复用已存在的 BSP_USB_CDC_Prepare：BSP_Platform_EnterDFU 现在顺序为 CDC Deinit → BSP USB 复位 200 ms、PHY 保持掉电且控制器时钟门控有效、数据线拉低 300 ms并恢复 → ROM 交接。删除原来只有 delay 300 ms 的做法，不引入另一套 GPIO 代码，不改变 main/任务/BSP/porting 边界，不修改第三方源码。同步 Prepare/BeginReset 接口契约，允许在停止设备栈后交接 ROM 前调用。

parser/task/boot/platform 四组主机测试通过，平台用例现验证 Deinit→Prepare→ROM 顺序。Debug/Release 构建通过，最新 Flash 分别 54744 B / 42.43%、26812 B / 20.78%，RAM 均 27904 B / 85.16%。

当前主机已显示 ROM DFU，用户可在 CubeProgrammer 选择 USB 并刷新连接，下载最新 HEX。尚未自动烧录修正版；必须在下载恢复 CDC 后再次发送 dfu，验证新固件无需寄存器干预即可枚举。DFU 实际下载/参数页比较/恢复运行仍待用户验收，未提交或推送。

### 修正版 DFU 闭环实板确认

用户保持 USB 全程连接，经 CubeProgrammer USB DFU 下载修正版，截图日志显示从 0x08000000 启动成功。随后主机当前设备列表确认 CDC COM11（0483/5740）恢复。用户再次从 CDC 发送 dfu，并反馈正常，确认修正版无需调试器寄存器干预即可重新进入 ROM DFU。

本次已验证：CDC 文本回显/uid/reboot；USB 持续连接下 ROM DFU 下载、恢复应用 CDC、再次软件进入 DFU。属于单次闭环及用户反馈，不扩展为多次循环或长时间稳定性结论。参数页下载前后逐字节比较、20 次重连/复位、10 分钟及 1 MiB 长测、任务栈水位仍未完成。未提交或推送。

## 阶段 5：联合验收执行记录

CLion 的 J-Link 配置启用 Persistent session，下载及 CDC 枚举正常；具体配置见 03_freertos_clion_debug.md。本阶段按用户授权执行联合验收。

### 连续传输发现的问题及修正

首次自动执行文本边界 29 项、串口重开 20 次、持续插线 reboot 20 次通过，但第一批连续多行数据发送超时。保持现场通过现有持久 J-Link GDB Server 读取：应用启动 SYS_OK、无 FreeRTOS 断言；CDC configured=true、rxReady=true、rxLength=64、rxOffset=0、txBusy=false，任务却已等待通知。不是枚举失败或缓冲越界。

原因：SendResponse 等待发送完成时可能已取走 RX_READY 通知；任务处理完当前应用分包后直接阻塞，而底层仍有下一包数据并保持 NAK，不会再产生 RX 中断。通知是唤醒提示，不能代替实际缓冲状态。

修正仅修改 app_usb_task.c：记录本轮是否消费输入；刚消费完当前分包时继续循环，再尝试读取底层待取数据，确认没有数据后才等待。没有增加轮询任务、缓冲区、公共接口或动态分配。新增真实任务主机用例模拟“发送时已消费 RX 通知但底层包仍待取”，修正前失败、修正后通过。现有 parser/task/boot/platform、启动和真实 CherryUSB 核心回归通过。

### 固件更新与参数页核验

- 短暂暂停读取现场后立即恢复；目标 Flash 与当时 Debug bin 逐字节一致。读取参数页 0x0801F800–0x0801FFFF，共 2048 B，先备份到忽略的 build/usb_stage5/。
- 为恢复旧任务的等待现场，通过持久服务器执行普通 reset/go，UID 再次核对正确后发送 dfu。CubeProgrammer CLI 2.23.0 枚举到唯一 ROM DFU：USB1、序列号 20543071484E、芯片 ID 0x468。
- 下载前校验 HEX 校验和及每个数据记录，地址仅覆盖 0x08000000–0x0800D5EB，排除参数页。DFU 另读一次参数页，与调试器备份相同；随后下载并校验修正版。工具日志明确仅擦除页 0–26，没有全片擦除或选项字节操作。
- 下载后通过 DFU 再读参数页，2048 B 前后完全一致；随后从 0x08000000 启动，恢复 CDC。当前参数页原本全为 FF，因此结论是本次空参数页未变化及实际擦除范围排除参数页，不扩展为已验证带业务参数的升级保留。
- 参数页 SHA256：d0ff1b294b5288d1ae1421eadf5b2d38a8752b76d472ff30bed9028e25b1c5b8。备份及下载/校验日志均只保留在本地忽略目录。

### 当前构建与边界

修正版 Debug Flash 54764 B / 42.44%，Release 26824 B / 20.79%，RAM 均 27904 B / 85.16%。未修改第三方库，分层及原有 64 B 背压/ZLP 契约保留。LED 硬件适配主机测试、MT6826S 协议主机测试通过；这些不等同于电机运行、编码器连续采样或 FOC 实时性实测。

人工 USB 拔插项按用户明确要求取消，不再作为待完成项；串口重开及持续插线 reboot 测试保留。修正版最终结果如下。

### 最终自动验收结果

测试运行于板上 Debug 固件；串口为 COM11，稳定 UID 为 00210040484E500220333031，ROM DFU 序列号为 20543071484E。以下均为实际执行结果。

| 核验项 | 结果 |
|---|---|
| 文本边界 | 29 项通过：1/63/64/65/127/128 B 及 CR/LF/CRLF；空行、未知行、精确匹配、129/300 B 超长恢复、迟到配对 LF、正文分段及多行合包 |
| 串口关闭/重开 | 20 次通过，每次检查 UID 与回显 |
| 持续插线 reboot | 20 次通过，每次重新打开端口、核对 UID 并继续回显，USB 全程未拔 |
| 持续回显 | 600.28 秒，3,534,960 B，逐字节核对一致，没有超时或重放；24 条 128 B 正文加 CRLF 作为连续批次 |
| USB 任务栈 | 最低剩余 736 B / 1024 B，满足至少 256 B；通过运行后栈填充水位扫描读取 |
| LED 任务栈 | 最低剩余 1800 B / 2048 B，任务运行计数有效 |
| 故障状态 | appStartupResult=SYS_OK，整机 STANDBY、无锁存故障，freertosAssertFile/Line 均为 0；CDC 已配置、无传输错误 |
| DFU 下载/校验 | 下载修正版成功、校验成功，只擦应用页 0–26，恢复 CDC 后参与全部长测 |
| 应用映像 | 长测结束再次进入 ROM DFU，直接读回 54764 B，与当前 Debug bin 逐字节相同，再从 0x08000000 恢复运行并核对 UID |
| 参数页 | 调试器备份、DFU 下载前/后及长测后的 2048 B 均一致；原值全 FF，限制见前节 |
| 软件回归 | parser/task/boot/platform、启动、真实 USB 核心/CDC、LED 硬件适配和 MT6826S 协议主机用例通过；Debug/Release 构建通过 |
| 人工拔插 | 用户明确取消，未执行，不作为失败或待完成项 |

栈和状态通过现有持久 J-Link GDB Server 在长测结束后短暂暂停读取，随后立即恢复；测试流量期间没有暂停。最终应用映像校验以 ROM DFU 直接读回为准：外部 DFU 更新后持久调试连接读取的 Flash 仍对应上一份基线缓存，未将该缓存用作新固件校验证据。RAM 水位及运行计数来自更新后目标，日志均保留。

原始证据位于本地忽略的 build/usb_stage5/：serial_results.json、summary.json、dfu_download.txt、dfu_flash_readback.txt、final_inspect.txt、参数页和栈快照。首次故障记录另保留为 serial_failure_baseline.json，避免覆盖失败证据。

本轮最终范围：CDC 文本、会话恢复、UID、普通复位及 ROM DFU 升级闭环通过上述验收；未引入 shell、FOC 控制或自建 Bootloader。Release 仅构建/接口检查，实板长测使用 Debug；真实业务参数保留、未来控制业务下的实时性不在本次结果中。按要求停止等待最终审核；阶段交付时未提交或推送，经用户审核后按四个提交入库（依赖固定 / USB 传输层 / 应用与 DFU / 文档）。
