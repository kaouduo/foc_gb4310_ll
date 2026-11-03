# FOC 审查问题修复与验证

更新日期：2026-11-02（Asia/Taipei）。对应 16 号审查记录的功能整改；未提交 Git。阶段 1～3 原有记录保留为历史，本文件描述整改后的实现。

## 实现结果

| 审查项 | 修复后的行为 |
| --- | --- |
| PWM 绕过保护 | `test pwm` 只在 Debug 提供，通过运行执行器排队、自动停机采零、验证母线后启动。ADC 有效性/序号、轨限、1.5 A 三相电流、8～27 V 母线、5 ms 反馈时效、硬件截止与有限租期保护均与开环共用。 |
| 生产零偏依赖诊断 | `App_Motor_PrepareZero → App_Motor_Acquire`，独占关闭输出采集 1000 点；不调用 Shell 测试入口。Debug 链路计算扩展移至 `app_motor_diagnostics.c`，Release 正常启动不依赖它。 |
| Release 携带软件测试 | `FOC_ENABLE_MOTOR_DIAGNOSTICS` 在 Debug 为 1，其余构建为 0；实现、命令解析、帮助、补全同步。完整数值枚举/模式夹具留在本地主机测试；Debug 的 `test foc` 改为三个已知向量冒烟检查。 |
| 重复停止/长关中断 | 两个公开停止名称共用一个实现。先立即关闭输出、发布取消，再由采样所有者停止硬件。收尾在短临界区认领，ADC 停止等待位于临界区外；认领标志防止任务抢占导致重复停止或提前重启。准备期由采零所有者响应取消，不重复停止 ADC。 |
| 遥测不可信 | `test foc` 输出 `hardware=untouched`。移除重复且可能陈旧的结果占空比字段，实际 PWM 以快照 CCR 为准；PWM 单独输出末帧电流，不把未采集的均值/范围打印成零。 |
| 预留实例与只写状态 | 只驻留当前使用的开环对象，移除电流/编码器电压对象、模式指针表与只写 dq 电流缓存。三种模式的算法源码及基类保留。 |
| 假通用 TIM 配置 | 删除 `MotorSTM32Config.timer`，PWM 与采样明确为 TIM1/ADC1/ADC2 专用实现；BSP 继续绑定引脚、通道和比例，不引入通用驱动管理层。 |
| 校准接口/常量重复 | 拟合函数改为文件私有，删除重复赋值；校准与参数有效性校验共用极对数质量常量。参数记录版本、64 B 格式、CRC/提交标记和 Flash 地址不变。 |

常用 `test enc/bus/current/zero/motor/stop` 在 Release 保留。`test sync/chain/foc/pwm` 仅 Debug 提供。日志等级不改变这些命令的编译开关。

IRQ 的有效性、截止检查和故障计数保留；硬件快照中的区间最值及耗时统计也暂时保留，便于现场检查，不再扩展诊断框架。没有削减运行保护，也没有接入新的电流闭环运行模式。

## 验证

- Debug：RAM 23,576 B，Flash 105,652 B；整改前为 23,696 B / 112,276 B。
- Release：RAM 23,512 B，Flash 64,184 B；整改前为 23,648 B / 73,444 B。
- 主机测试：43,230 组 SVPWM/变换/PI/斜坡/三模式；校准 1/7/14/64 极对、正反方向和失败质量门禁；Flash 64 B 各字节损坏、不完整提交、忙门禁；PWM 映射/舍入/租期/截止；运行取消、故障锁存和不自动重启均通过。
- 新增定向覆盖：PWM 过流/轨限/母线过期立即关断，欠压拒绝启用；停止调用时全局中断开放且只停止一次，停止等待中重入监督任务不重复收尾；Release 不带高级诊断仍可自动采零并完成开环。
- 49 个自有 C/H 格式检查、UTF-8/LF、C++23 公共头及差异空白检查通过。
- 2026-11-02 完成 Debug → Release → Debug 三轮短实板回归，分别 28/27/31 项检查通过。包含准备/带电校准取消、重复请求拒绝、帮助/补全、校准保存、1 秒开环、重启加载及不自动启动。
- 三轮均为 14 极对、方向 −1，零偏分别 4.461/4.468/4.467 rad；校准约 17.4 s，峰值电流最大 0.898 A，校准 IRQ 最大 2081 cycles（170 MHz 下约 12.24 μs），硬件错误为零。
- 第三轮受保护 `test pwm u` 完成 20,000 帧，含采零约 1051 ms，CCR 为 850/2125/2125，ADC/截止错误均为零，结束输出/采样关闭。实际过流/轨限/反馈过期注入属于主机测试，本次未故意制造实板过流。
- Release 高级诊断及帮助关闭，生产采零、开环和校准正常。三轮参数页 CRC、64 B 记录后的擦除区检查通过，重启加载参数一致。
- CPU 故障寄存器为零、CCER 为零、MOE 关闭。2048 B 栈最少未触及区：USB 532 B、电机 832 B、LED 1208 B；填充图案只反映本次执行路径。空闲堆 8184 B，历史最低 6008 B。
- 最终 Debug Flash 读回与 ELF 全部可加载节一致；105,652 B 地址范围中仅向量表与 text 间 8 B 非加载间隙为擦除态 FF，而 bin 对该空洞补零。当前保留 Debug，输出/采样关闭，串口释放，未提交 Git。

### 本次实板发现及修正

首次关闭输出的 `test chain` 首帧触发截止错误：新拆出的诊断文件漏了原快环的 `-O2`，IRQ 4031 cycles、预算 4011 cycles。已将 `app_motor_diagnostics.c` 加入既有实时优化源列表，再开始正式三轮，均通过。初次失败记录 `round1-debug.*` 不计入通过轮次。

测试工具也做了修正：Windows GDB 的 CP1252 字符集警告影响符号表达式读取，改为 nm 解析地址后读取原始内存；PWM 命令约一秒后返回，脚本原 180 ms 静默窗口过早判失败，改为等待结束。第一轮已完成的固件检查保留，快照/重启部分续测通过；第三轮的等待脚本尝试保留为 `attempt3-harness.*`，当时尚未开始该轮完整校准，不算额外正式轮次。

正式证据：`final1-debug.*`（含 `final1-run.log`、`final1-resume.log`）、`final2-release.*`、`final3-debug.*`、`final-results.json` 和 `final-verify.log`。

本地证据在 `build/foc-fix/`，测试脚本在被忽略的 `tests/`，不加入仓库。用户原有 `03_freertos_clion_debug.md` 与整理前 SHA256 一致。

## 最终修改目录

下列累计清单相对当前 HEAD，覆盖阶段 1～3、规范整理和本次整改；排除用户原有的 `03_freertos_clion_debug.md`。A 为新增、M 为修改、D 为删除。清单由实际 Git 状态生成，便于最终核对。

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
