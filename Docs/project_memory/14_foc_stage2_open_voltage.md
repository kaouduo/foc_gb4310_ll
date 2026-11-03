# FOC 阶段 2：受保护开环电压运行与验证

更新日期：2026-10-31（Asia/Taipei）。状态：已实现，最终三轮短时回归通过，等待用户评估。本阶段接续阶段 1，开放有限时间的旋转电压运行，不开放已标定电角度电压模式或实际电流闭环。用户已授权当前 24 V 台架直接进行与参考工程相近的测试，不再重复要求断开电机。

## 1. 运行接口与执行流程

| 命令 | 行为 |
|---|---|
| motor open | 排队请求默认电频率 2 Hz、1 V、3000 ms、200 ms 上下电压斜坡 |
| motor open Hz V duration_ms ramp_ms | 可选参数必须四个一起提供；十进制支持符号和最多 6 位小数，不支持科学计数法 |
| motor stop | 同步关闭输出与采样，取消准备中的启动；无需等待完整运行时限 |
| motor status | 状态、上次结果、故障、周期数、母线、动态电压上限、电流、编码器和 IRQ 统计 |
| motor clear | 空闲且母线恢复有效后清除本模块锁存故障，不清其他来源故障，不自动重启 |

电频率范围 ±100 Hz（0 可用于有限时间静态矢量），幅值必须正且有限；时长 100～10000 ms，斜坡至少 1 ms 且不超过总时长的一半。电压没有固定 8 V 上限，启动时按当前母线可用范围约束指令，运行时持续按母线动态限幅。参数解析使用有界循环，避免 strtof 引入大块 libc 和堆分配。

```text
USB/Shell：motor open → 参数检查 → 固定大小命令事务 → queued
1 ms motor 任务：preparing → 检查母线 → 输出关闭采集 1000 次零偏
  → 更新母线与编码器缓存 → 配置/enter 已绑定开环模式
  → 中断受控事务内开启采样与有限周期输出 → open
ADC IRQ：样本/时效/电流保护 → FOC_Step → sin/cos → 逆 Park → SVPWM → CCR
  → 最后 200 ms 降压 → 达到 60000 个周期时立即关断 → stopping
motor 任务：停采样/exit → idle → 输出一次结果日志
```

默认 3 s 从实际允许输出开始计算，包含上升与下降斜坡，不包含约 51 ms 的零偏准备。`OK ... queued` 只表示请求接受；实际启动以 state=open / 启动日志为准。只有停机后才能提交下一次运行。

新增任务为静态 motor，优先级 3、栈 2048 B；USB 保持优先级 2，LED 为 1。任务不执行快速电流控制，错过监督周期后跳过旧周期，不连续追赶积压的 tick。

## 2. 保护、绑定与共享数据

- BSP 继续只 bind：新增有限周期 enable_output 与保存/恢复中断掩码的短事务能力，实际实现都在 porting。波形诊断 enable_test_output 仍限制 1 s，正常运行租期最多 10 s。
- TIM1 更新 IRQ 独立倒计时输出租期；ADC 中断中的正常时限和保护直接关闭 MOE/相通道，不经过 RTOS 队列。
- 母线工作范围 8～27 V；可用 dq 电压为 `2*min(0.5-Dmin,Dmax-0.5)*Vbus/√3`。监督任务约每 1 ms 读取一次规则组 ADC，以同一事务发布母线、限幅值和 DWT 时间戳。
- 任一 U/V/W 相瞬时电流幅值超过 1.5 A，或 ADC 原始值小于 16 / 大于 4079，立即关断并锁存过流故障；W 为重建相。
- 母线样本超过 5 ms 未更新，由 ADC IRQ 关断；超过 5 ms 没有快速回调，由 supervisor 关断。样本序号/标志、底层 ADC/截止错误及意外输出关闭同样产生故障。
- 首故障记录原因、统一 Sys_Status、故障位、周期和原始 U/V；日志在任务输出。故障后不得重启，必须显式 clear，再发送 open。
- 编码器只用于本阶段开环遥测，由 supervisor 读取；运行中 test enc 返回缓存，test bus 返回已发布母线，避免 Shell 争用 SPI/规则组 ADC。编码器失败计数可见，但不冒充开环模式所需的角度反馈。
- 运行/准备/排队期间拒绝原有采样/PWM 诊断；实际电机模式对象不被纯软件 test foc 的夹具修改。
- mode/input 状态事务通过 BSP 能力保存/恢复原中断掩码，不调用 RTOS。停机时在受控事务内关闭输出、停 ADC 并退出对象；底层硬件停转换等待有 1 ms 超时，正常路径为短等待，错误路径也不会继续输出。
- Shell 只提交启动命令，supervisor 编排启动，显式 stop 可同步执行受控关断/退出；IRQ 独占运行期间的模式计算状态，其他读取使用受控快照。ISR 不打印、不申请内存、不使用 FreeRTOS API。

状态快照保留末次电压命令、电流采样及峰值；idle 时应以 output=0/sampling=0 判断输出关闭，冻结的采样值不是此刻电压/电流实测值。

reboot、dfu 和原有 test stop 均经过 App_Motor_Stop。启动准备期间的 stop 会使请求代次失效，完成零偏收尾后不允许旧请求再次开启输出。

## 3. 本阶段验证要求与结果边界

主机检查覆盖：完整默认 60000 周期、手动停止、准备期间取消、重复启动拒绝、诊断互斥、过流/ADC 轨边/欠压/反馈过期/快速回调缺失/截止错误、显式清除与无自动重启。另用大电压请求和母线下降验证动态限幅可以高于或低于 8 V，而不是固定 8 V。故障注入仅在主机模拟，未故意制造实板过流或电气故障。

首轮台架探索已观察到默认 2 Hz/1 V/3 s 正常完成、峰值约 170 mA，机械位移约 -127°；这是编码器实际运动观测，不用于宣称极对数或电角度已校准。

最终版本已完成 Debug → Release → Debug 三轮短时回归，包含：

- 参数拒绝、阶段 1 zero/sync/chain 回归、默认完整运行、运行中缓存读取与重复启动/诊断拒绝。
- 手动停止、准备期间取消、唯一前缀 Tab 补全、-2 Hz/1 V/1000 ms 反向运行、clear 后保持停止、test foc。
- Release 额外检查运行期间 reboot 后回到空闲且不自动启动；DFU 沿用同一同步停机入口，本轮不重复 ROM DFU 交接验收。
- 读回参数页、CFSR/HFSR、TIM1 输出、任务数、堆及 USB/LED/motor 栈余量。

### 回归发现并修复的时间戳竞争

首次 Release 回归在第 4240 个周期锁存 stale（status=-2、fault=0x80）并关断输出。GDB 原始状态和 MOE=0 确认了现场。Task 原来使用 `DWT_now() - last_sample_cycles`：读取 now 后若插入 ADC IRQ，右侧时间戳变成更晚值，无符号减法下溢，误判超时。

主机在时间读取函数内插入一帧有效 ADC 回调，原实现稳定得到 state=fault/reason=stale。修复为两个明确语句，先读取 volatile 的 last_sample，再读取 now；同一测试保持 state=open、faults=0。最终三轮必须使用修复后的固件，前面的探索/失败不计入通过结果。

PWM 写入同时只允许 ±1e-6 的浮点舍入修正，避免有效调制边界因浮点误差触发内部故障；超过该容差仍报错关断。独立硬件模拟检查覆盖通道映射、该边界、租期和截止保护。

| 轮次 | 默认周期/峰值 mA | 最大运行 IRQ cycles / μs | 最小截止预算 cycles | 正向/反向机械位移 counts |
|---|---|---|---|---|
| round1-debug | 60000 / 170 | 2031 / 11.95 | 3963 | -13082 / 2993 |
| round2-release | 60000 / 169 | 2044 / 12.02 | 3980 | -13073 / 2941 |
| round3-debug | 60000 / 170 | 2031 / 11.95 | 3977 | -11592 / 3080 |

| 轮次 | 最低空闲堆 B | USB 未用栈 B | LED 未用栈 B | motor 未用栈 B |
|---|---|---|---|---|
| round1-debug | 6016 | 512 | 1208 | 1096 |
| round2-release | 6008 | 716 | 1256 | 1080 |
| round3-debug | 6016 | 496 | 1208 | 1096 |

最终 Debug：RAM 23224 B / 70.87%，Flash 104248 B / 80.80%；Release：RAM 23176 B / 70.73%，Flash 65720 B / 50.94%。

三轮 CFSR/HFSR 均为 0，结束时 TIM1 CCER=0、MOE=0，任务数 5。默认运行正向指电频率 +2 Hz，编码器计数为负；反向 -2 Hz 计数为正，不能据此跳过方向/零位校准。栈值为填充值未用区，不等同所有未来调用路径最坏余量。Release 在 reboot 后又显式运行 1 s，栈快照包含该次运行。

参数页 SHA256：`d0ff1b294b5288d1ae1421eadf5b2d38a8752b76d472ff30bed9028e25b1c5b8`，与阶段 2 下载前快照逐字节一致；本阶段没有 Flash 标定写入。

最终重建后另通过 GDB compare-sections 核对板端 .isr_vector/.text/.rodata，并逐字节核对 Flash 中 .data 初值，与最终 Debug 镜像一致。CLion 本轮暂停请求超时，故障现场使用现有 J-Link/GDB 批读取；没有在功率输出期间暂停 CPU。

本地证据：`build/foc-stage2/results.json`、`round*.serial.log`、`round*.snapshot.log`、任务栈和参数二进制快照。失败探索保留在 `issue-release-stale.serial.log`；最终三轮汇总不混入失败结果。当前恢复 Debug，电机输出和采样关闭，串口已释放。

母线计算值约 25 V，与用户所述 24 V 台架仍需对照万用表及 VDDA/分压实际值；本轮不把软件毫伏值视为已校准的绝对电压。真实电流增益/相序辨识、正反机械一圈的编码器校准、Flash 参数保存和电流闭环上电调参仍属于后续阶段。

## 4. 最终修改目录

本阶段新增：

```text
project/application/motor/app_motor_internal.h   共享内部对象，保留三种模式的唯一实例
project/application/motor/app_motor_runtime.c    supervisor/IRQ 桥接、开环生命周期与保护
project/application/task/app_motor_task.c        1 ms 静态任务入口
project/application/shell/commands/cmd_motor.c   motor 命令及有界十进制解析
Docs/project_memory/14_foc_stage2_open_voltage.md 本记录
```

已有文件的职责变更：main/app_task 增加静态任务编排；app_motor 增加运行分派、准备零偏与诊断互斥；app_shell/cmd_test 增加补全及缓存访问；motor_io/bsp_motor/PWM/sampling 增加并绑定租期运行和状态事务能力；CMake 增加源文件和实时 O2；项目记忆同步当前状态。

以下为相对当前 Git HEAD 的累计实际文件清单，包含尚未提交的阶段 1，排除用户原有 03_freertos_clion_debug.md 改动，便于核对整个交付：

```text
M  CMakeLists.txt
M  Docs/project_memory/02_session_context.md
M  Docs/project_memory/04_architecture.md
M  Docs/project_memory/05_platform_rtos.md
A  Docs/project_memory/12_foc_design.md
A  Docs/project_memory/13_foc_stage1_validation.md
A  Docs/project_memory/14_foc_stage2_open_voltage.md
M  Docs/project_memory/README.md
M  project/application/main.c
A  project/application/motor/app_motor.c
A  project/application/motor/app_motor.h
A  project/application/motor/app_motor_internal.h
A  project/application/motor/app_motor_runtime.c
M  project/application/shell/app_shell.c
A  project/application/shell/commands/cmd_motor.c
M  project/application/shell/commands/cmd_system.c
M  project/application/shell/commands/cmd_test.c
A  project/application/task/app_motor_task.c
M  project/application/task/app_task.h
M  project/bsp/bsp_board_config.h
M  project/bsp/encoder/bsp_encoder.c
A  project/bsp/motor/bsp_motor.c
A  project/bsp/motor/bsp_motor.h
A  project/common/motor_io.h
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
```

A=新增，M=修改，D=删除；累计 56 个文件。

`tests/foc_stage2_*` 和 `build/foc-stage2/` 为本地检查与证据，遵守既有不入库约定。没有自动提交或推送。
