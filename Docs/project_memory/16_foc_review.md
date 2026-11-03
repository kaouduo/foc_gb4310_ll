# FOC 全部变动审查与规范整理

更新日期：2026-11-01（Asia/Taipei）。范围：相对当前 HEAD 的阶段1～3全部未提交实现，以及对应构建、文档和删除项。用户原有 `03_freertos_clion_debug.md` 只读检查并校验未修改。

后续功能整改见 [17_foc_fixes.md](17_foc_fixes.md)。本文件以下为整改前审查历史，“待整改”不作为当前实现状态。

本轮完成审查及注释/格式整理，**未实施下列功能重构、未改变控制参数、未删除已验收命令、未烧录或操作电机、未提交 Git**。阶段3实板记录仍是功能验证历史，不能称为本次规范整理后的再次实板验收。

## 1. 优先处理的问题

### P1：裸 PWM 诊断绕过正常电流与母线保护

位置：[app_motor.c](../../project/application/motor/app_motor.c) 的 `App_Motor_FastStep`、`Diagnose`；[cmd_test.c](../../project/application/shell/commands/cmd_test.c) 的 `test pwm`；CMake 无条件编译该命令。

诊断分支直接写固定三相占空比，采样回调主要收集统计和检查 ADC/截止错误。它没有走 `App_Motor_RuntimeFastStep` 中的 1.5 A、电流轨限和 8～27 V 母线保护。独立一秒租期只限制时长，不能代替这些保护，Release 中也可以调用。原有已确认台架授权没有改变这一实现事实。

建议先将裸 PWM 波形入口默认限制在 Debug；凡允许带电执行的诊断，应接入同一套电流/母线检查与立即关断。不能靠帮助文本“断开电机”替代代码保护，也不能把正常保护作为“过度诊断”删除。

状态：待功能整改，本轮只补清接口契约。

### P2：生产零偏采集依赖完整诊断执行器

位置：`app_motor.c` 的 `App_Motor_PrepareZero → Diagnose(APP_MOTOR_TEST_ZERO)`。

同一文件同时承担初始化、IRQ分派、零偏准备、采样诊断、固定PWM测试和729项软件自检。`App_MotorContext` 也同时保存模式状态与诊断统计。直接给整个诊断实现加 Debug 开关会破坏正常开环及校准启动所需的零偏采集。

建议保留一个用于生产的最小停机采样/零偏过程，诊断调用该过程；诊断实现集中到一个 `app_motor_diagnostics.c` 或 Shell 下的诊断文件。核心只保留必要的诊断采样分派，不新增诊断管理器或多层接口。

状态：待功能整改。

### P2：软件单元测试与详细诊断默认进入 Release

位置：`app_motor.c: App_Motor_TestAlgorithms`、`cmd_test.c`、`app_shell.c` 补全表、CMake。

Release 仍包含360角度枚举、非法值检查、临时三模式实例与测试PI增益。ARM Release符号表中 `App_Motor_TestAlgorithms` 本体为952 B，尚未计入其引用的正余弦等代码。它适合验证，不是正常运行必需业务。

建议：完整数值枚举放本地测试；Debug 的 `test foc` 可保留少量已知向量冒烟检查。`test enc/bus/current/zero` 保留为常用定位能力，`test sync/chain/pwm` 作为调试诊断。用一个明确编译开关同步控制实现、命令、帮助及补全，避免半开关状态。

状态：待功能整改，未擅自删除用户已经验收的命令。

### P2：停止入口重复，且硬件等待位于全局关中断区

位置：`app_motor.c: App_Motor_Stop`；`app_motor_runtime.c: App_Motor_RequestStop/Finish`；`motor_sampling_stm32.c: MotorSTM32_Stop/WaitClear`。

运行时 `App_Motor_Stop → RequestStop → Finish` 已停止采样，外层又关断并停止一次；准备阶段也存在重复停止。`Finish` 在 `critical_enter` 后调用停止采样，ADC异常时该调用可能轮询至1000 μs，与“短状态事务”的接口意图不符。

建议收敛为一个停止入口，保证立即关断幂等；将状态所有权交接留在短临界区，将已禁止输出/控制IRQ后的硬件停止等待放到临界区外。调整前须覆盖准备取消和故障IRQ交错，不能只删几行调用。

状态：待功能整改。此处是错误路径最长等待风险，不表示正常停止每次都会耗时1 ms。

### P2：诊断输出含未反映真实状态的数据

位置：`cmd_test.c` 的 `test foc`，以及 `app_motor.c: Diagnose`。

`test foc` 固定打印 `output=off`，但它本身不停止正在运行的电机，文字容易被理解为真实硬件状态。`Diagnose` 的PWM分支修改了实际占空比，却没有同步更新 `last_duty`，因此 `App_MotorTestResult.duty_*` 仍可能报告初始0.5。这两项属于诊断可信度问题。

建议将软件自检输出标为“不访问硬件”，或读取真实硬件状态；PWM实际占空比以写入成功值/硬件CCR为准，移除没有消费者的重复结果字段。

状态：待功能整改，本轮注释明确软件自检不保证电机已停止。

## 2. 可以精简的状态和接口

1. `motor.modes[1/2]` 只在初始化写入，运行执行器始终访问 `[0]`。当前驻留的 `FOC_Current` 为80 B、`FOC_EncoderVoltage` 为32 B、模式指针表为12 B，合计124 B。算法与基类是用户明确要求，应保留；实际实例可等对应模式接入时再构造，或真正使用有名字的当前模式句柄。不要删除整个电流环算法来省这点RAM。
2. `last_id_a/last_iq_a` 合计8 B，仅写入、无正常代码读取；属于诊断遗留。应删除或建立明确的遥测消费者。加上上述124 B，当前可识别的这部分驻留冗余约132 B，并非当前RAM的大头。
3. `MotorSTM32Config.timer` 看起来可选择任意定时器，但初始化、IRQ、故障关断和多数访问固定TIM1。PWM写入又使用配置指针，抽象不一致。本轮已补“TIM1专用”契约；后续可直接简化专用配置，不必为了本板引入通用定时器框架。
4. `MotorCal_Fit` 只被本实现调用，却对外声明；可以改为文件私有函数。`MotorCal_*` 本身是单实例状态机，这符合当前单轴需求，无须为未来多轴提前增加对象管理层。
5. `FitLeg` 有重复赋值，部分质量门槛如0.20/0.15在校准和存储检查两处重复；可统一小范围常量。`test stop`/`motor stop`、`test motor`/`motor status` 有用途重合，可保留兼容别名，不需要两套状态实现。
6. ADC IRQ 中区间最值等统计属于诊断；执行截止检查、采样有效性和故障计数属于保护。将来精简Release统计时必须分开处理，不能一起删掉。

这些属于后续精简建议，本轮没有改变对象布局、生命周期或现有命令行为。

## 3. 当前架构中合理的部分

- BSP只绑定静态硬件资源；porting没有回引application，未为简单MCU外设再造空壳driver层。
- TIM1时基/CH4及ADC1/2由一个采样模块拥有，移除原两个GPIO占位初始化是合理合并。
- PI、斜坡、Clarke/Park、SVPWM与三种FOC算法无BSP/LL/RTOS依赖；首成员基类和静态虚表符合已确认设计。
- 校准流程、运行保护、记录序列化分工合理；没有新增控制快环任务、堆分配或多后端配置框架。
- 非有限值、ADC硬件错误、超时、CRC、最后有效标记和“先关断再记故障”是有效边界检查，不作为冗余删除。
- 单页单记录符合本轮约定，掉电回滚和不可纠正ECC恢复没有实现；保留已有文档边界，不扩成双页存储系统。

## 4. 本轮已经修改的规范问题

- `.clang-format` 明确禁止单行函数体/单行if及循环，增加控制语句花括号。原配置允许部分压缩写法，所以“工具检查通过”并不足以保证阅读一致性。
- 格式化当前变动中的48个自有C/H文件，按`.editorconfig`统一UTF-8/LF；第三方文件未改。
- 展开压缩文件头，补齐接口参数、返回值、单位、调用线程/IRQ、所有权和失败副作用；补充FOC输入输出、PI增益、校准质量字段及硬件能力说明。
- 修正`RequestOpen`注释误挂在校准接口前、硬件能力“只有写占空比/关断可以IRQ调用”漏掉临界事务等注释不一致。
- 为porting声明和能力指针补参数名，明确TIM1/ADC1/ADC2为专用实现。
- 校验用户原有03文档的SHA256与整理前一致；未修改第三方、本地调试连接或参数页。

本轮没有实施上述P1/P2功能整改；不把补注释描述为补上了保护。

## 5. 验证和当前边界

- Debug/Release构建通过；48文件`clang-format --dry-run --Werror`、`git diff --check`和公共头C++23语法检查通过。
- 主机集成回归覆盖默认60000帧、停止/准备取消、故障与不自动重启；校准覆盖1/7/14/64极对、双向、迟滞与异常；存储覆盖往返、64字节逐一损坏和不完整提交，均通过。
- 规范整理后资源不变：Debug RAM23696 B/Flash112276 B；Release RAM23648 B/Flash73444 B。
- 本轮未上板，板端仍是阶段3三轮实测版本；未把这次构建结果写成新的实板通过记录。
- 本地辅助与证据位于`tests/foc_review_docs.py`、`build/foc-review/`，遵守不入库约定。未暂存、提交或推送。

## 6. 修改目录与后续顺序

本轮修改`.clang-format`、既有48个C/H文件的注释/排版，以及项目记忆；新增本审查记录。未新增产品源文件、未删除产品接口。当前相对HEAD的累计实际文件清单附后，阶段3记录内62文件清单保留为阶段3历史快照。

建议后续先解决带电诊断保护和停止事务，再拆开生产零偏与诊断，将Release自检收敛，最后清理未使用状态。每一步都应保持现有校准/停止行为可回归，不同时重写整个电机执行器。

### 当前相对 HEAD 的累计实际文件清单

A新增、M修改、D删除；排除用户原有03文档，共65个文件。

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
