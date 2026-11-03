# FOC 阶段 3：编码器校准与单页参数保存

更新日期：2026-11-01（Asia/Taipei）。状态：阶段3实现完成，最终Debug → Release → Debug三轮短时实板检查通过，最终Debug镜像与板端核对一致；输出/采样关闭，COM11已释放。最终功能验收由用户评估。未提交或推送Git。

## 1. 本阶段能力与命令

```text
motor cal start        显式启动 2 Hz / 5 V 编码器校准
motor cal status       当前阶段、失败位置、质量数据及已保存参数
motor cal stop         同 motor stop，立即关闭输出并取消流程
motor clear            条件恢复后清除故障，不自动重启
```

增加 motor cal 及其 start/status/stop 参数补全。非法参数拒绝；已有运行或诊断时拒绝并发启动。命令返回 queued 表示排队成功，不能当作校准通过。

status 的 stored 是当前应用持有的上次有效结果；candidate 是本次尝试。失败不会将 candidate 当成有效参数覆盖 stored。保存失败后旧 RAM 参数可以保留 valid，但 saved 清零，表示 Flash 状态无法保证。重启只加载通过结构、版本、CRC 和质量字段检查的记录，不自动运行；启动后的流程 phase=idle 代表本次尚未校准，不代表存储参数无效。

本阶段不开放实际编码器电压模式或电流闭环命令，不提供未校准反馈的替代值。原 motor open 仍限制最多 10 s；电压继续受实时母线及调制范围动态限幅，不添加固定 8 V 上限。

## 2. 流程与层次

BSP 只 bind 硬件能力，未增加控制策略。校准状态机在 application/motor，独立于寄存器、RTOS 和 Flash；仍复用唯一 FOC_OpenVoltage 实例及既有快速保护。没有新增任务、动态堆分配或额外管理台。

1. supervisor 校验母线、关闭输出采集1000点电流零偏，并读取首个编码器样本。
2. align：电角度保持0，5 V从0按200 ms斜坡上升；保持合计800 ms。
3. map_move/map_hold：2 Hz移动约120°电角度，静止保持500 ms，检查第二组电流通道方向。
4. return/zero_hold：−2 Hz返回约0°电角度，静止保持500 ms，记录初始机械角平均值与实际电角度。
5. forward：2 Hz正向扫描，依据展开编码器位移到达一机械圈（32768 counts）结束。每约1024 counts收集一点，最多33点；去除前100 ms启动暂态后拟合有符号极对数，至少16点。
6. forward_realign/forward_hold：以2 Hz的较短方向回到初始电角度，额外电角行程不超过半圈，再保持500 ms，记录正向停靠零位。
7. reverse：−2 Hz反向扫描一机械圈，独立拟合并检查极对数、方向一致性。根据两方向全圈轨迹分别求零偏圆周均值，再取两方向均值的圆周中心，抵消运动迟滞。
8. reverse_realign/reverse_hold：回到初始电角度并保持500 ms；以两方向静态停靠零位的圆周中心与初始零位比较，另检查端点跨度和静态观测与全圈零偏的差。
9. ramp_down：200 ms电压下降，然后关闭PWM、停止采样、退出算法对象。
10. saving：停机擦写并读回，通过后phase=done，发布有效参数。

一机械圈的终点是首次观测到绝对位移不少于 32768 counts，因此存在最多一个 supervisor 采样周期附近的行程过冲。除正反各一机械圈外，还有对齐、通道检查及两次回归初始电角度的小范围机械移动，不将通道检查混为扫描行程。

轨迹拟合 θ_command = slope × θ_mechanical + intercept。极对数取 |slope| 最近整数，direction 取 slope 符号。零偏使用两方向完整轨迹的圆周均值；三次静态观测用于质量检查。轨迹的绝对机械角包含启动原点，避免只用相对行程造成重启后零偏错误：

`θe = wrap(direction × pole_pairs × θm + offset)`。

不会硬编码 14 极对，也不会根据普通开环的三秒行程推测极对数。不能仅凭此流程确认物理 UVW 标签或自动修复任意接线；当前 U/V/W=CH1/CH3/CH2 及 ADC2 U负、ADC1 V正的板级映射保持既定定义。

## 3. 本轮明确的质量和保护阈值

| 检查 | 通过条件 |
|---|---|
| 支持极对数 | 1～64 |
| 正/反单向拟合 | 与最近整数相差 ≤0.20 极对；对拟合直线的电角度残差 RMS ≤0.35 rad |
| 两方向一致性 | 最近整数相同、方向相同、拟合值差 ≤0.15 极对 |
| 静态稳定性 | 保持末 100 ms 至少取得 50 个 supervisor 样本，机械计数跨度 ≤32 counts |
| 电流通道一致性 | 0°及约120°电角度，投影电流 >20 mA，投影能量占比 >85%（约23°方向误差范围） |
| 正反全圈零偏 | 两方向圆周均值差 ≤0.5 rad |
| 静态返回检查 | 正反停靠中心与初始零位差 ≤0.25 rad；三端点最大圆周差 ≤0.5 rad |
| 静态/全圈检查 | 任一静态观测与全圈零偏的圆周差 ≤0.75 rad |
| 反馈错误 | 一次失败、设备状态异常或一次读数跳变达到128 counts 即关断 |
| 无运动 | 每500 ms 机械位移绝对值至少32 counts |
| 单向时间上限 | 正向未知 p 用64；反向用已辨识 p；上限 = (p/2 Hz)×1.25+0.5 s |
| 总时间上限 | 按2×64/2 Hz×1.25+4 s计算为84 s，不使用固定80 s替代阶段估计 |
| 既有快速保护 | 三相估算峰值1.5 A、ADC轨限、母线8～27 V、母线/采样最大5 ms年龄、序号与截止时间错误 |

上述阈值是本轮台架的工程门槛。拟合残差不是标定后电角度的绝对精度，本阶段未实现编码器非线性查表补偿。电压和电流绝对增益仍受名义 VDDA、分压、分流/放大误差影响。W 是重建电流，能量/相序检查只证明两路与已知激励的基本一致性，不等于独立验证三路电流传感器。

硬件输出租期能力上限由10 s扩到90 s，供84 s最坏校准范围使用；test PWM仍最多1 s，普通open仍最多10 s。租期由TIM1更新中断执行，与ADC控制回调分开；应用按阶段超时与无运动检查提前结束。达到校准总帧数会按超时故障关断，不能伪装成校准成功。

阶段计算在supervisor中运行，ISR只处理命令交接和既有开环/保护。任务发布频率/电压到固定大小待处理结构，短临界区发布标志，IRQ在下一周期应用。阶段计算期间可能被故障IRQ打断，因此应用动作前在临界区重新检查运行状态，不能覆盖已锁存的关断状态。

## 4. 持久化记录

继续独占最后2 KB单页：0x0801F800～0x0801FFFF，单记录64 B，其余保持擦除。字段按STM32小端编码，浮点为32位IEEE754；不直接保存运行对象或指针。

| 偏移（B） | 字段 |
|---|---|
| 0/4/8 | magic、版本2、记录长度64 |
| 12/16/20 | uint32极对数、int32方向、float零偏rad |
| 24/28/32/36 | 正/反拟合极对数、正/反RMS rad |
| 40/44 | 板级映射schema、保留0 |
| 48/52 | CRC32（覆盖0～47字节，IEEE反射多项式0xEDB88320）、保留0 |
| 56/60 | 最后写入的双字有效标记 |

版本1的早期静态局部零偏记录不沿用，版本2采用正反全圈均值。

保存顺序：确认输出与采样均关闭→擦页→写前56 B→最后写8 B标记→重新加载与逐字段重编码比对。地址/长度按8 B对齐，字段不必每个占8 B。空白、坏CRC、版本/schema不符、坏标记、非法物理字段均视为未校准，不自动擦写修复。

校准失败/取消不擦写，因此原Flash记录保留。真正开始单页保存后若掉电或写失败，旧记录可能丢失；此设计不保证旧值回滚，可读取但不完整的记录在下次启动被拒绝。现有Flash porting不恢复不可纠正ECC；若硬件报此类异常，沿用NMI紧急关断路径，不能把CRC检查描述成ECC恢复。没有增加日志页、双页或磨损均衡。电流零偏每次启动功率运行重新采集，不持久化短期ADC零偏。

## 5. 验证记录

已完成主机检查：1/7/14/64极对及方向±1；有动态滞后的合成轨迹；无运动、非整数极对、畸变残差、反向电流和反馈失败拒绝；全运行仿真校准成功后先停机再自动保存；排队/带电阶段取消、过流与后续反馈失败不覆盖有效结果。运行集成O0/O2通过。记录验证覆盖往返、64个字节逐一损坏、不完整提交、运行中拒绝保存和非法参数。PWM夹具验证1 s/90 s租期及84 s合法范围。公共头C++23语法检查通过。

当前构建：Debug RAM23696 B（72.31%）、Flash112276 B（87.02%）；Release RAM23648 B（72.17%）、Flash73444 B（56.92%）。相对阶段2 Debug，RAM增加472 B，无新增任务栈。

实板探索揭示两类质量判断问题：静止起点混入运动拟合使反向RMS为0.26505 rad；排除前100 ms后仍观察到整圈平滑起伏，某轮RMS为0.25016 rad。因此将工程残差门槛明确设为0.35 rad，并将最终零偏改为正反整圈圆周均值。不能据此判定起伏来自编码器本体、安装或机械负载，也不宣称已完成线性化。

停靠端点受迟滞影响，实测端点最大差约0.276 rad。每圈结束回到初始电角度后，按参考工程的双向停靠取中心方法检查返回误差（0.25 rad），同时保留0.5 rad端点跨度和0.75 rad静态/全圈差上限。调试期间拒绝的结果均关断输出且未覆盖有效记录。收尾修正了return_error状态打印缺少一个参数的问题，为本命令增加编译器格式检查及串口数值断言。

探索日志/参数快照独立保留在exploration、before-*和issue-*文件；不计作最终三轮通过结果。最终三轮使用相同控制实现，按Debug→Release→Debug执行；最终结果如下。

本地检查脚本在tests/foc_stage3_*及扩展后的foc_stage2_runtime.c、foc_stage2_pwm.c，证据输出在build/foc-stage3/；按约定不入库。实板与主机检查分别记录，不互相替代。


### 最终三轮实板结果

三轮均为14极对、direction=−1；正反行程各不少于32768 counts，保存后重启加载相同参数且不自动运行。以下偏置以Shell四舍五入到mrad显示，跨三轮为1 mrad差，不等于绝对角度精度。

| 轮次 | 检查数 | 校准ms | 零偏mrad | 拟合极对（正/反） | RMS mrad（正/反） | 返回中心误差mrad | 峰值mA | 最大IRQ cycles（μs） | 最小截止预算cycles |
|---|---:|---:|---:|---|---|---:|---:|---|---:|
| round1-debug | 23 | 17416 | 4469 | 13.895 / 13.905 | 183 / 201 | 19 | 880 | 2082（12.247） | 3929 |
| round2-release | 22 | 17394 | 4468 | 13.938 / 13.921 | 238 / 237 | 4 | 877 | 2099（12.347） | 3923 |
| round3-debug | 23 | 17398 | 4468 | 14.041 / 14.016 | 248 / 284 | 8 | 876 | 2082（12.247） | 3965 |

每轮覆盖：帮助/非法参数/三级补全、排队取消、输出启用后的取消、重复启动BUSY、完整校准与自动保存、质量输出数值断言、输出/采样关闭、算法729项自检、普通1秒开环、重启后加载且不自启。两轮Debug另核对日志dropped=0。ADC/控制截止等硬件错误为0，CFSR/HFSR为0，最终CCER=0、BDTR=0x0C33（MOE=0）。

三轮任务数均5；最低堆余量Debug6016 B、Release6008 B。经历校准/保存后的未触及栈区最小值：USB556 B、motor872 B、LED1208 B。此数为栈填充值扫描的实测余量，不宣称覆盖所有异常路径。

通过GDB compare-sections核对最终Debug的.isr_vector/.text/.rodata；另逐字节比对Flash中的312 B .data初值与最终bin尾部一致。参数页记录版本2、CRC32、最后双字提交标记正确，剩余1984 B为0xFF；最终参数页与第三轮保存快照相同。

最终参数页SHA256：`c753011f4371a77ee395851556c940597ebe5698e798a8901f5e37691c734572`。

本轮没有人为制造实板过流/断电中断写入；这些失败路径使用主机夹具验证，不能写成真实故障注入通过。没有安排长时间测试。故障调试时CLion无活动嵌入式会话，使用已存在的J-Link服务器做停机后GDB读取；刷新寄存器缓存后PC在FreeRTOS idle路径，未在功率输出期间暂停CPU。

原参数页备份在build/foc-stage3/parameter-before.bin；最终三轮独立输出round*.json、round*.serial.log、round*.snapshot.log及栈/参数快照，汇总results.json，镜像核对verify-final.log。探索结果不混入该汇总。

## 6. 本阶段实际修改目录

新增：

```text
project/application/motor/app_motor_calibration.h   校准类型、动作、进度与质量接口
project/application/motor/app_motor_calibration.c   任务状态机、轨迹拟合和静态零偏
project/application/motor/app_motor_storage.h       校准记录加载/保存接口
project/application/motor/app_motor_storage.c       64 B序列化、CRC、最后提交与读回
Docs/project_memory/15_foc_stage3_calibration.md      本阶段设计与验证记录
```

修改：

```text
CMakeLists.txt                                     添加校准/存储源文件，保留实时源O2
project/application/motor/app_motor.h               校准命令、快照接口与故障原因
project/application/motor/app_motor_runtime.c       校准生命周期、IRQ命令交接、自动保存/加载
project/application/shell/commands/cmd_motor.c      cal start/status/stop、中文帮助及质量输出
project/application/shell/app_shell.c               cal二三级补全
project/common/storage_layout.h                    参数记录位置、长度、版本
project/common/motor_io.h                          输出租期能力契约更新为最长90 s
project/porting/pwm_output/pwm_output_stm32.c        有限租期范围扩到90 s
Docs/project_memory/12_foc_design.md                当前阶段链接
Docs/project_memory/README.md                       索引和当前状态
Docs/project_memory/02_session_context.md            当前能力与未验证边界
```

阶段1/2累计清单以14文档为历史快照；本阶段累计最终实际清单附后。用户原有03_freertos_clion_debug.md修改未处理。

### 相对当前 HEAD 的累计实际文件清单

包含未提交阶段1～3，排除用户原有03文件，共62个文件；A新增、M修改、D删除。

```text
M  CMakeLists.txt
M  Docs/project_memory/02_session_context.md
M  Docs/project_memory/04_architecture.md
M  Docs/project_memory/05_platform_rtos.md
A  Docs/project_memory/12_foc_design.md
A  Docs/project_memory/13_foc_stage1_validation.md
A  Docs/project_memory/14_foc_stage2_open_voltage.md
A  Docs/project_memory/15_foc_stage3_calibration.md
M  Docs/project_memory/README.md
M  project/application/main.c
A  project/application/motor/app_motor.c
A  project/application/motor/app_motor.h
A  project/application/motor/app_motor_calibration.c
A  project/application/motor/app_motor_calibration.h
A  project/application/motor/app_motor_internal.h
A  project/application/motor/app_motor_runtime.c
A  project/application/motor/app_motor_storage.c
A  project/application/motor/app_motor_storage.h
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
M  project/common/storage_layout.h
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
