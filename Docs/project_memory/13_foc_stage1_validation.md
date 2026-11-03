# FOC 阶段 1 实施与验证记录

更新日期：2026-10-31（Asia/Taipei）。本记录保留阶段 1 快照，后续源码与累计清单见 [阶段 2](14_foc_stage2_open_voltage.md)。本阶段实现电机硬件 BSP、FOC 算法与绑定，提供逐项 test 检查；没有开放持续电机运行和实际闭环控制。最终验收由用户评估，不自动 Git 提交。

## 1. 实际绑定与执行顺序

```text
main：Clock/DWT → BSP_Motor_Bind → BSP_Motor.init
  → TIM1/安全 GPIO/ADC 校准，保持采样与六路功率输出关闭
startup：编码器/Flash 初始化 → App_Motor_Init
  → 构造 OpenVoltage/EncoderVoltage/Current 三个静态子类
  → 持有各子类首成员 FOC*，绑定静态 const 虚函数表
  → BSP_Motor.set_sample_callback(App_Motor_FastStep, &motor)
  → 正常 Shell/日志启动
test：启动有限次采样 → IRQ 同步回调 → 返回任务打印结果 → 关断输出并停止采样
```

BSP 只保存板级静态配置、porting 上下文和能力表。porting 不引用 BSP 或 application。PI、变换、SVPWM 和模式不访问硬件、RTOS 或日志。

硬件统一资源拥有者是 motor_sampling：TIM1 时基、CH4 与 ADC1/2 只初始化一次。PWM 模块只写 CH1～3 的预装载和控制输出开关，不重新复位 TIM1。

## 2. 已实现的硬件与算法

- TIM1 中心对齐，PSC=0、ARR=4250、RCR=1，目标 20 kHz；300 ns 对应初始死区编码 51。逻辑 U/V/W 绑定 CH1/CH3/CH2。
- CH4 PWM2，CCR4=ARR-80，TRGO2=OC4REF 上升沿；主 ADC1 单一完成 IRQ 读取成对的 ADC1/2 注入结果。ADC 时钟 PCLK/4，电流采样 12.5 周期。
- ADC2_IN17 为负极性 U，ADC1_IN3 为正极性 V，W=-U-V。ADC1 规则组 IN14 读取母线，247.5 周期；允许在同步注入采样期间由唯一任务读取。
- 样本序号、同步完成/溢出、IRQ 间隔及耗时统计；区分 ADC 错误与截止时间错误。截止预算取 IRQ 入口实际下数 CNT，预留 128 个周期；错误立即关闭 MOE、相通道并停触发，避免中断连续占满 CPU。
- 三相 CCR 写入期间使用 UDIS 防止部分预装载同时生效；ISR 写入前检查定时器方向与剩余预算。
- PWM 波形测试使用独立 TIM1 更新 IRQ 租期，最多 20000 个周期；ADC 回调不再到达也能计时关断。正常启动不开输出。
- CPU Fault/NMI 使用不依赖栈的汇编入口先关闭 MOE/六路输出；FreeRTOS assert/栈溢出/分配失败路径接入同样的底层关断。实际异常注入和功率波形尚未验收。
- 独立 PI（位置式、积分边界、联合限幅后回算）、标量/二维斜坡、Clarke/Park/逆 Park、SVPWM、角度归一化及快速 sin/cos；未设置固定 8 V 限制。
- 两种电压模式及电流模式共用 enter/step/exit；编码器模式要求有效标定；电流模式另要求合法 PI 配置与有效电流。实际电机实例没有假标定/假 PI 参数。

### 电压与时间约定

`Ulimit = 2 × min(0.5-Dmin, Dmax-0.5) × Vbus / √3`。当前 5%～95% 时为 `0.9 × Vbus / √3`。

普通开环默认 2 Hz、1 V、200 ms 斜坡；3 s 运行期限留给后续正常运行流程。本阶段 chain 仅运行约 50 ms，保持功率输出关闭。它在开始前读取母线并固定本次干运行输入，不冒充已经实现持续运行的母线年龄管理。

### 实时路径调整及证据

初始 Debug/O0 全链路约 59 μs，超出控制周期。后续收紧到实际下数更新截止预算，不能用“不到 50 μs”代替“赶上本次更新”。

最终措施：实时源文件独立 O2，Debug 保留 g3/日志；开启 Flash 预取；快速路径不用带重复分类开销的 fminf/fmaxf；sin/cos 使用四象限约简和多项式，主机与标准库比较绝对误差小于 2e-6；周期不变的量纲系数预先计算。

编码器 SPI 频率由 BSP 配置 /16，即名义 10.625 MHz，仍为 Mode 3、8 bit。原厂 [MT6826S Rev.1.1](https://www.magntek.com.cn/upload/pdf/202407/MT6826S_Datasheet_Rev.1.1.pdf) 给出时序约束；本频率满足其要求。SPI 以最多两个未接收字节流水收发，保留整帧超时、CRC、状态检查与错误恢复。

以上耗时只覆盖本次开环干运行链路。完整电流 PI 加入实际 ISR 后必须重新量测，不能把本阶段余量作为所有后续模式的时间保证。

## 3. 逐项 test 命令与检查含义

| 命令 | 行为与检查 | 验证边界 |
|---|---|---|
| test motor | 初始化、采样、输出开关、TIM/错误计数 | 应 init=1、空闲 sampling=0/output=0 |
| test enc | 单次角度/CRC/状态 | 已验证通信，不代表电角度已校准 |
| test bus | 原始 ADC 与换算毫伏值 | 已验证转换，绝对精度需对照万用表 |
| test current | 关闭输出，采集 1000 次 U/V 原始值 | zero=0 时电流值不具备零偏校正意义 |
| test zero | 无电流且输出关闭时采集 RAM 零偏 | 均值 100～3995、峰峰值不超过 50 counts；不写 Flash |
| test sync | 1000 次同步回调，同时轮询读取规则组母线 | bus_reads>0、无 ADC/截止错误、间隔约 8500 core cycles |
| test chain | 编码器→Clarke/Park→开环模式→逆 Park→SVPWM→CCR | 先 test zero；功率输出一直关闭；没有已标定角度和实际 PI 调节 |
| test foc | 729 项板端纯软件检查 | 三种模式的绑定/门禁及软件夹具，不驱动实际电机 |
| test pwm u/v/w/all | 按用户授权台架直接输出 1 s，随后关闭 | 已执行四项；实际波形由用户此前硬件验证，本次未直接采集示波器 |
| test stop | 关闭输出、停止采样 | 诊断结束状态可再用 test motor 检查 |

PWM 诊断占空比：默认各相 50%；u 将 U 改为 20%，v 将 V 改为 20%，w 将 W 改为 80%，all 使用 U/V/W=20%/50%/80%。这些是限时固定占空比检查，不是旋转电压开环运行命令。用户已明确授权当前台架直接执行，替代先前必须断开电机的测试前提。

test 二级命令和 pwm 三级相名支持 Tab；错误参数不启动测试。reboot/dfu 请求前先停止电机硬件。由于测试同步占用 Shell 任务，不能依赖从同一终端输入 stop 中断正在执行的 1 s 测试；自动关断由底层租期保障。

## 4. 验证方法与边界

- 主机：43,230 个母线/角度 SVPWM 点、坐标往返、sin/cos 精度、矢量限幅、零目标斜坡、PI 饱和/恢复、三模式状态与非法值；使用简单动态被控对象检查电流 PI 恢复。
- 绑定：模拟采样回调/context，验证零偏、输出关闭的 chain、有限 PWM 路径、样本异常/无采样超时及退出清理。
- 接口：C23 固件与 C++23 公共头编译；Debug/Release 无新增编译告警。
- 板端：COM11、已有 J-Link 服务 4444；最后三轮为 Debug→Release→Debug。CLion 启动未建立有效会话，采用现有 GDB Server 的批处理连接，没有新建 J-Link 服务。
- 每轮检查 help、编码器、母线、current、未校准/未置零 chain 拒绝、zero、sync、chain、foc、Tab、错误参数、UID、时间、输出关闭；Debug 另检查日志无丢弃。
- 三轮后读回参数页并逐字节比较；检查 CFSR/HFSR=0、TIM1 CCER=0、BDTR.MOE=0、堆最低空闲与任务栈填充值。

用户说明接入 24 V 后，计算母线约 25.0 V；不能据此判定电压标定正确。分压实际阻值、VDDA 与万用表读数需继续对照。U/V 极性和相序仍依据确认的板级配置，未用带电受控激励验证电流映射。

## 5. 未实施及待验收

- 用户已确认硬件已验证、当前台架可直接测试 PWM。四项有效 PWM 命令已执行，结果如下；此前 test pwm invalid 仅是参数拒绝测试。
- 三相通道/互补/频率/死区的外部硬件波形结论由用户提供。本次软件已验证 CCR、同步采样、限时结束及输出关闭，没有直接采集示波器或重新证明功率桥的电气指标。
- 后续实现正常运行 supervisor、命令交接、母线年龄管理、完整故障快照/保护、正反机械圈校准、Flash 标定与实际电流闭环调参。
- 没有进行长期测试，没有自动提交/推送。保留用户原有 03_freertos_clion_debug.md 改动。

### PWM 台架追加验证

用户授权后在现有 Debug 固件运行，未改固件、未扩大到持续电机控制：

| 命令 | 样本数 | 耗时 ms | CCR U/V/W | 硬件错误 | 结束输出 |
|---|---|---|---|---|---|
| test pwm u | 20000 | 1001 | 850 / 2125 / 2125 | 0 | off |
| test pwm v | 20000 | 1001 | 2125 / 850 / 2125 | 0 | off |
| test pwm w | 20000 | 1001 | 2125 / 2125 / 3400 | 0 | off |
| test pwm all | 20000 | 1001 | 850 / 2125 / 3400 | 0 | off |

四项 ADC/deadline 错误均为 0，最大 IRQ 254 cycles（约 1.49 μs）；最终 test motor 为 sampling=0/output=0，并补发 test stop 后释放 COM11。ADC 原始值随激励发生变化；此时 zero=0，因此输出中的 current_mA=0 不代表实际电流为零，也不能用来确认电流量纲和映射已标定。

证据：`build/foc-stage1/pwm.serial.log`，本地脚本 `tests/foc_stage1_pwm.ps1`。没有新增固件修改，前述最终三轮构建/非功率链路结果仍对应当前固件。

## 6. 最终资源与三轮结果

Debug：RAM 20936 B / 63.89%，Flash 97444 B / 75.52%。Release：RAM 20888 B / 63.75%，Flash 59768 B / 46.32%。

RTOS 堆调整为 8192 B；没有新增任务或永久测试采样数组。三轮启动峰值堆余量和任务栈余量见下方汇总。栈余量为填充值未使用区域，不等同完整调用路径最坏栈深。

| 轮次 | 最大 IRQ cycles / μs | 最小截止预算 cycles | 最低空闲堆 B | USB/LED 未用栈 B |
|---|---|---|---|---|
| final1-debug | 3731 / 21.95 | 4008 | 6008 | 556/1792 |
| final2-release | 3695 / 21.74 | 3995 | 6016 | 880/1888 |
| final3-debug | 3731 / 21.95 | 3995 | 6008 | 556/1848 |

三轮每项采集 1000 次，回调/硬件/编码器错误均为 0；sync 的并行规则组母线读取成功。三轮 CFSR/HFSR 均为 0，功率输出和采样退出后关闭。

参数页 SHA256：`d0ff1b294b5288d1ae1421eadf5b2d38a8752b76d472ff30bed9028e25b1c5b8`，与下载前快照逐字节相同。

本地证据：`build/foc-stage1/results.json`、`final*.serial.log`、`final*.snapshot.log` 和栈/参数二进制快照；本地测试脚本 `tests/foc_stage1_*` 遵守不入库约定。

## 7. 最终实际修改目录

下列清单由最终工作区差异生成，排除用户原有的 03_freertos_clion_debug.md；与设计文档第 7 节的后续目标分开。

```text
M  CMakeLists.txt
M  Docs/project_memory/02_session_context.md
M  Docs/project_memory/04_architecture.md
M  Docs/project_memory/05_platform_rtos.md
A  Docs/project_memory/12_foc_design.md
A  Docs/project_memory/13_foc_stage1_validation.md
M  Docs/project_memory/README.md
M  project/application/main.c
A  project/application/motor/app_motor.c
A  project/application/motor/app_motor.h
M  project/application/shell/app_shell.c
M  project/application/shell/commands/cmd_system.c
M  project/application/shell/commands/cmd_test.c
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

A=新增，M=修改，D=删除；共 50 个文件。
