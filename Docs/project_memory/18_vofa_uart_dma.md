# USART3 DMA 与 JustFloat 遥测

更新日期：2026-11-02（Asia/Taipei）。状态：实现完成，Debug→Release→Debug 三轮短实板检查通过；未提交 Git。

## 当前配置与使用

- VOFA+ 打开 **COM9、115200、8N1、无流控**，协议选择 **JustFloat**。UART TX 为 PC10/AF7，接收器与板卡共地。只实现发送，不使用 PC11 接收，也不提供 UART Shell。
- USB CDC 的 COM11 继续用于 CherrySH 和日志，两条输出独立；可以同时连接。
- `vofa` 为持久静态任务，优先级 1、栈 1024 B，每 1 ms 检查一次新快照。20 kHz 控制 IRQ 每 200 个成功控制帧发布一次，最高 100 Hz；一帧 68 B，平均约 6.8 kB/s。
- 实际开环或校准计算期间发送；停机、准备采零、固定 PWM 测试或计算失败时不发布新帧。停机后可能完成最后一帧，不持续重发缓存，也不伪造全零帧。
- 可用 COM11 的 `motor open 2 1 1000 200` 观察 1 秒波形。任务不会自行启动电机；原运行保护、启动门禁、持续时间与停止命令保留。

JustFloat 帧为 16 个小端 IEEE754 binary32，加 `00 00 80 7F`。序号只在固件中用于识别新数据和统计跳过，不额外添加未经协议约定的帧头/CRC。VOFA 时间轴采样率设为 100 Hz；这是抽取后的遥测，不能当作完整的 20 kHz 电流波形。

| 通道（VOFA 从 0 开始） | 内容 | 单位 |
| --- | --- | --- |
| 0/1/2 | 本周期 Iu/Iv/Iw，Iw = −Iu−Iv | A |
| 3 | FOC 输入母线电压 | V |
| 4 | FOC 输入 dq 矢量幅值上限 | V |
| 5 | FOC 输入控制周期 dt，当前 0.00005 | s |
| 6/7 | FOC_Input.id_a / iq_a；当前开环未计算，实际传入值为 0 | A |
| 8 | FOC 输出调制电角度 | rad |
| 9/10 | FOC 输出 Ud/Uq | V |
| 11/12 | 逆 Park 输出 Uα/Uβ | V |
| 13/14/15 | SVPWM 逻辑 U/V/W 占空比 | 0～1 |

6/7 不代表已实现电流闭环或已计算实际 dq 反馈。后续接入反馈时应由控制执行器提供真实输入，遥测不再计算一份控制算法。

## 实现职责与并发

- `middleware/vofa` 沿用参考工程 `VOFA_JustFloat_Build` 的纯组帧职责和小端格式，用 memcpy 读取浮点位模式，并检查 binary32 编译条件。没有移植旧 `Write/VOFA_Port_Write` 发送落点。
- application 在成功写入本周期 PWM 后分频发布同周期输入、输出、电压、占空比和电流。IRQ 不调用 RTOS、日志、UART 或等待锁。任务用短临界区复制完整快照，最新值优先，无长队列。
- `app_vofa_task.c` 只发送新序号，组帧并调用 BSP 提交。UART 忙时丢本帧，不等待、不补发旧数据；统计 submitted/skipped/busy/errors 供调试器查看。
- BSP 只绑定静态 porting 实例、板级引脚、时钟、波特率及 IRQ 优先级。USART3/PCLK1、DMA1 Channel1、DMAMUX USART3_TX 为专用实现，无通用外设管理器。
- porting 拥有独立 68 B DMA 缓冲。非阻塞提交先复制，调用方返回即可复用源缓冲；普通模式 DMA、字节对齐、内存递增、硬件低优先级。DMA IRQ 优先级 6，低于控制采样 IRQ 4，不调用 RTOS。
- 完成 IRQ 停止通道、清标志、累计计数、释放缓冲；完成表示 DMA 不再读取缓冲，不等于 UART TC 或主机已收到。DMA 错误立即禁用请求并锁存。后续提交发现缓冲占用超过 20 ms 时锁存超时；没有额外后台看门狗。错误后需要重新初始化/重启恢复。
- 提交检查与缓冲交接仅屏蔽自身 DMA IRQ，保持控制 IRQ 开放。遥测初始化失败只记录独立状态并不创建遥测任务，不解除或额外锁存电机故障。

## 验证与实际边界

最初按 921600/1 kHz 测试：固件侧 DMA 完成 998 帧、无错误，COM9 收到 60739 B 且帧内缺字节。921600/500 Hz 和 460800/500 Hz 也未获得完整数据。COM9 是 J-Link CDC UART；这些结果只证明该台架接收链路在相应配置下不可靠，尚未确定具体硬件/固件瓶颈。保留失败原始数据，没有靠扫描帧尾掩盖丢字节。

最终选择 115200/100 Hz。可配置位置：波特率在 `bsp_board_config.h`，分频在 `app_motor_telemetry.h`，两者必须结合帧长度、接收器能力和实测吞吐调整。

正式三轮均运行 2 Hz/1 V/1000 ms/200 ms 斜坡，并在 COM9 捕获原始数据，同时由 COM11 查询状态：

| 轮次 | 接收 | DMA 完成/接受 | 跳过/忙/错误 | 控制 IRQ 最大 cycles |
| --- | --- | --- | --- | --- |
| Debug 1 | 99 帧 / 6732 B | 99 / 99 | 0 / 0 / 0 | 2688 |
| Release | 99 帧 / 6732 B | 99 / 99 | 0 / 0 / 0 | 2664 |
| Debug 3 | 99 帧 / 6732 B | 99 / 99 | 0 / 0 / 0 | 2688 |

结束帧先关闭输出、不再执行调制，所以 1 秒运行发布 99 帧，而不是强制补齐 100 帧。逐帧检查了帧长/帧尾、16 通道有限值、电流和、dt、动态限幅、逆 Park 与 SVPWM 一致性。不是只凭收到若干字节判通过；DMA 接受/完成数也与完整接收帧数对应。

三轮 CPU 故障寄存器为零，输出/采样关闭、DMA 无占用。最大控制 IRQ 约 15.81 μs，采样/截止错误为零。三轮 Flash 参数页逐字节相同；本次未重做完整自动校准，不替代已有校准验收。

遥测任务本次最大栈使用上界 316 B，1024 B 栈至少 708 B 未触及；这里只反映本次路径。任务数为 6；空闲堆 8176～8184 B，历史最低 6008 B。RAM 相对上一修复版增加 1304 B：

| 构建 | RAM | Flash |
| --- | --- | --- |
| Debug | 24880 B / 32 KB（75.93%） | 109180 B / 126 KB |
| Release | 24816 B / 32 KB（75.73%） | 65444 B / 126 KB |

主机 JustFloat 已知字节、负零、最大通道、容量/哨兵检查通过；控制/校准/存储回归及 C++23 公共头检查通过。收尾时用户并行修改了 SVPWM 实现，主机编译发现 NULL 缺少声明；经用户确认保留该实现，仅补 `<stddef.h>` 后回归和双构建再次通过。最终 Debug Flash 读回 109180 B 与当前构建完全一致（空洞按擦除态填充），说明该头文件补齐不改变已验证机器码。实际抓取和工具脚本仅在本地 `build/vofa/`、`tests/`，不入库。未声称通过 VOFA GUI 本身作了验收，接收证据为 COM9 原始字节与协议数值校验。

当前保留 Debug，电机输出/采样关闭，COM9/COM11 释放。没有提交或推送。

## 本次最终修改目录

```text
A project/common/uart_tx.h                         异步发送能力及统计
A project/bsp/vofa/bsp_vofa.h / .c                 UART3 板级静态绑定
M project/bsp/bsp_board_config.h                   波特率及 DMA IRQ 优先级
M project/porting/uart/uart_stm32.h / .c            替换 GPIO 占位为 TX DMA
A project/middleware/vofa/vofa_justfloat.h / .c      独立协议组帧
A project/application/motor/app_motor_telemetry.h  同周期快照契约
M project/application/motor/app_motor_internal.h   内部发布接口
M project/application/motor/app_motor.c            发布/读取快照
M project/application/motor/app_motor_runtime.c    成功控制周期分频采集
A project/application/task/app_vofa_task.c         最新帧组帧发送
M project/application/task/app_task.h              新任务声明
M project/application/main.c                       静态任务及启动初始化
M CMakeLists.txt                                  源码及头文件目录
M project/middleware/foc/foc_svpwm.c                保留用户实现，仅补 stddef.h
M Docs/project_memory/04_architecture.md           UART 架构接续
M Docs/project_memory/05_platform_rtos.md          遥测任务与 IRQ 接续
A Docs/project_memory/18_vofa_uart_dma.md          本记录
M Docs/project_memory/README.md                    索引与最新状态
M Docs/project_memory/02_session_context.md        接续记忆
```

此前阶段 1～3、规范整改的历史目录见 17 号记录；以下累计实际清单由相对 HEAD 的 Git 状态生成，排除用户原有的 03 文档。

<!-- FILE_MANIFEST -->

```text
M  .clang-format
M  CMakeLists.txt
M  Docs/project_memory/02_session_context.md
M  Docs/project_memory/04_architecture.md
M  Docs/project_memory/05_platform_rtos.md
M  Docs/project_memory/07_interface_rules.md
A  Docs/project_memory/12_foc_design.md
A  Docs/project_memory/13_foc_stage1_validation.md
A  Docs/project_memory/14_foc_stage2_open_voltage.md
A  Docs/project_memory/15_foc_stage3_calibration.md
A  Docs/project_memory/16_foc_review.md
A  Docs/project_memory/17_foc_fixes.md
A  Docs/project_memory/18_vofa_uart_dma.md
M  Docs/project_memory/README.md
M  project/application/main.c
A  project/application/motor/app_motor.c
A  project/application/motor/app_motor.h
A  project/application/motor/app_motor_calibration.c
A  project/application/motor/app_motor_calibration.h
A  project/application/motor/app_motor_diagnostics.c
A  project/application/motor/app_motor_internal.h
A  project/application/motor/app_motor_runtime.c
A  project/application/motor/app_motor_storage.c
A  project/application/motor/app_motor_storage.h
A  project/application/motor/app_motor_telemetry.h
M  project/application/shell/app_shell.c
A  project/application/shell/commands/cmd_motor.c
M  project/application/shell/commands/cmd_system.c
M  project/application/shell/commands/cmd_test.c
A  project/application/task/app_motor_task.c
M  project/application/task/app_task.h
A  project/application/task/app_vofa_task.c
M  project/bsp/bsp_board_config.h
M  project/bsp/encoder/bsp_encoder.c
A  project/bsp/motor/bsp_motor.c
A  project/bsp/motor/bsp_motor.h
A  project/bsp/vofa/bsp_vofa.c
A  project/bsp/vofa/bsp_vofa.h
A  project/common/motor_io.h
M  project/common/storage_layout.h
A  project/common/uart_tx.h
A  project/middleware/control/pi.c
A  project/middleware/control/pi.h
A  project/middleware/control/ramp.c
A  project/middleware/control/ramp.h
A  project/middleware/foc/foc_base.c
A  project/middleware/foc/foc_base.h
A  project/middleware/foc/foc_current.c
A  project/middleware/foc/foc_current.h
A  project/middleware/foc/foc_encoder_voltage.c
A  project/middleware/foc/foc_encoder_voltage.h
A  project/middleware/foc/foc_math.c
A  project/middleware/foc/foc_math.h
A  project/middleware/foc/foc_open_voltage.c
A  project/middleware/foc/foc_open_voltage.h
A  project/middleware/foc/foc_svpwm.c
A  project/middleware/foc/foc_svpwm.h
A  project/middleware/foc/foc_transform.c
A  project/middleware/foc/foc_transform.h
A  project/middleware/vofa/vofa_justfloat.c
A  project/middleware/vofa/vofa_justfloat.h
D  project/porting/adc_regular/bus_voltage_stm32.c
D  project/porting/adc_regular/bus_voltage_stm32.h
D  project/porting/current_sampling/current_sampling_stm32.c
D  project/porting/current_sampling/current_sampling_stm32.h
M  project/porting/freertos/FreeRTOSConfig.h
M  project/porting/freertos/freertos_hooks.c
A  project/porting/motor_sampling/motor_sampling_stm32.c
A  project/porting/motor_sampling/motor_sampling_stm32.h
M  project/porting/mt6826s/mt6826s_spi.c
M  project/porting/mt6826s/mt6826s_spi.h
M  project/porting/pwm_output/pwm_output_stm32.c
M  project/porting/pwm_output/pwm_output_stm32.h
A  project/porting/stm32g4/motor_fault_shutdown.S
M  project/porting/stm32g4/system_clock.c
M  project/porting/uart/uart_stm32.c
M  project/porting/uart/uart_stm32.h
```
