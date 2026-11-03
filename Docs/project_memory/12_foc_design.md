# FOC 基类、算法与硬件绑定设计

更新日期：2026-11-01（Asia/Taipei）。状态：阶段 1 已实现 BSP 硬件接口、三种 FOC 算法模式和绑定，最终三轮非功率链路检查通过，PWM 限时台架追加测试通过；外部波形沿用用户硬件验证；实板范围与实际文件清单见 [阶段 1 记录](13_foc_stage1_validation.md)。本文同时保留后续目标，不能将未实施部分视为已有能力。

### 阶段 2 当前交付

已新增有限时间开环运行、1 ms supervisor、母线动态限幅、快速电流/时效保护、故障锁存与清除，以及运行中编码器缓存访问。参数、测试与实际累计清单见 [阶段 2 记录](14_foc_stage2_open_voltage.md)。编码器标定、Flash 保存与实际电流闭环仍为后续目标。

### 阶段 3 当前实现

编码器正反机械圈校准、电流通道一致性检查、极对数/方向/静态零偏辨识及单页64 B记录已接入。阈值、状态机、实际修改清单及验证边界见 [阶段 3 记录](15_foc_stage3_calibration.md)。最终Debug→Release→Debug三轮实板通过，当前Debug镜像已核对、输出/采样关闭。最终功能验收由用户评估；实机编码器电压/电流模式等后续目标不能当成已验证能力。

### 本次阶段 1 交付边界

- 实现 TIM1 三相互补输出、CH4 触发、双 ADC 注入同步采样、并行规则组母线读取、回调注册，以及 BSP 的静态资源绑定。
- 实现独立 PI、斜坡、角度/sin-cos、Clarke/Park/逆 Park、SVPWM，以及三个首成员基类模式；application 构造实例并注册 ADC 快速回调。
- 增加 test 命令逐条诊断：编码器、母线、电流/零偏、同步采样、输出关闭的全链路、软件算法、状态与限时 PWM 波形。
- 不开放电机持续运行命令，不将软件夹具的标定标志或 PI 参数用于实际电机。校准、Flash 记录、完整运行保护和带电电流闭环留到后续阶段。
- 阶段 1 不新增 supervisor 任务；诊断命令在现有 USB/Shell 任务中启动短时测试，实际快速计算仍在采样 IRQ 内。后续正常运行才引入 supervisor。

## 1. 目标与边界

本轮设计包含开环旋转电压、编码器角度电压、编码器角度电流闭环三种模式。统一使用 SVPWM。速度环、位置环、无感估算、弱磁与解耦前馈不在本轮实现范围内。

| 项目 | 约定 |
|---|---|
| 普通开环默认值 | 电频率 2 Hz、电压幅值 1 V、持续 3 s、电压斜坡 200 ms |
| 电压限制 | 根据实时有效母线电压和允许的调制范围动态计算 dq 矢量幅值上限，不设置固定 8 V 上限 |
| 控制频率 | 目标 20 kHz，每周期 50 μs；实际触发次数和输出生效时刻必须实测 |
| 初始占空比范围 | 5%～95%；必须结合 ADC 有效采样窗口验证，不能仅凭占空比范围判断采样有效 |
| PWM | TIM1 中心对齐，初始死区目标 300 ns，U/V/W 对应 CH1/CH3/CH2 |
| 电流采样 | ADC2 为 U 相、负极性；ADC1 为 V 相、正极性；W 相重建 |
| 换算参数 | 分流电阻 0.05 Ω、增益 20、参考电压名义 3.3 V、12 位 ADC |
| 母线采样 | 75 kΩ / 10 kΩ 分压；实际参考电压误差另由测量验证 |
| 校准 | 自动辨识极对数、角度方向、零偏；正反各一机械圈；另检查电流采样映射 |
| 参数保存 | 现有末尾 2 KB Flash 单页、单记录；成功校准后停机保存 |
| 内存 | 静态控制对象，不增加快速环任务；RTOS 堆由 16 KB 调整为 8 KB前须核算启动峰值 |

运行电压模式和闭环电流模式均不等于已经具备电流安全保障。电流有效性、母线保护、采样截止时间及停机路径必须先完成，才能带电调试。

## 2. 层次与职责

| 层 | 职责 | 不承担 |
|---|---|---|
| application | 电机生命周期、模式对象组装、公共快速执行器、保护、校准、参数保存、命令及遥测 | LL 寄存器访问、在 ISR 打印 |
| bsp | bind 板级资源、porting 实例和 driver；公开薄接口或已绑定能力表 | PI、变换、SVPWM、校准和运行策略 |
| porting | GPIO/TIM/ADC 初始化、中断、寄存器收发、PWM 紧急关闭、回调保存与调用 | 直接引用 application 或 BSP 的实现符号 |
| driver | 维持编码器等器件协议；不新增仅包装 MCU 外设的空壳驱动 | 电机业务流程 |
| middleware | FOC 基类与模式、PI、斜坡、坐标变换、SVPWM | BSP、HAL/LL、RTOS、日志、Flash |
| common | 跨层类型、统一错误码、板级持久化布局 | 算法实现或外设操作 |

FOC 与现有 LED 一样使用“基类首成员＋静态常量虚函数表”。FOC 是可复用算法，放 middleware；LED 的硬件子类做法不意味着将 FOC 模式放进 BSP。

**绑定分工：** BSP 绑定电机硬件资源；各 FOC 子类的 Bind 构造算法对象；application 选择模式并关联控制执行器。三者不混用。

## 3. 基类及模式对象

### 3.1 公共数据和接口

所有物理量采用 A、V、rad、s。电角度按 [0, 2π) 归一化。使用现有 Sys_Status，不新增 FOC_Result；故障原因由 application 的故障快照记录。

```c
typedef struct FOC FOC;

typedef struct {
    float dt_s;
    float bus_voltage_v;
    float voltage_limit_v;       /* 本周期可用 dq 矢量幅值 */
    float electrical_angle_rad;  /* 标定后的编码器电角度 */
    float id_a;
    float iq_a;
    bool encoder_valid;
    bool current_valid;
    bool calibration_valid;
} FOC_Input;

typedef struct {
    float electrical_angle_rad;
    float ud_v;
    float uq_v;
} FOC_Output;

typedef struct {
    Sys_Status (*enter)(FOC *self, const FOC_Input *input);
    Sys_Status (*step)(FOC *self, const FOC_Input *input,
                       FOC_Output *output);
    void (*exit)(FOC *self);
} FOC_Ops;

struct FOC {
    const FOC_Ops *ops;
};

void FOC_Base_Bind(FOC *self, const FOC_Ops *ops);
Sys_Status FOC_Enter(FOC *self, const FOC_Input *input);
Sys_Status FOC_Step(FOC *self, const FOC_Input *input,
                    FOC_Output *output);
void FOC_Exit(FOC *self);
```

- Bind 仅构造，不操作外设；虚表运行中不替换，所有对象静态存储。
- enter 检查模式前提、复位局部状态；电压/电流从零斜坡起步。
- step 每个控制周期调用一次，执行时间有界；成功才允许使用 output，失败立即关闭输出。
- exit 清理状态，不承担安全关断；公共执行器先关闭 PWM，再退出模式。
- 模式不持有硬件接口、任务句柄、Flash 服务和日志输出。
- 所有模式只能停机后切换，不实现运行中无扰切换。

### 3.2 子类与参数

| 子类 | 命令 | 私有运行状态 | step 行为 |
|---|---|---|---|
| FOC_OpenVoltage | 电频率 Hz、电压 V、斜坡时间 s | 积分电角度、当前电压、斜坡进度 | 积分角度，输出 Ud=幅值、Uq=0；负频率反转 |
| FOC_EncoderVoltage | Ud、Uq、斜坡时间 | 当前 dq 电压与斜坡进度 | 使用输入电角度，按矢量斜坡输出电压 |
| FOC_Current | Id、Iq、电流斜坡 A/s | 两个 PI、当前 dq 电流指令 | 使用输入 Id/Iq 与角度，计算限幅和抗饱和后的 dq 电压 |

各子类包含首成员 FOC base，提供专用 Bind 和类型明确的命令设置接口；不使用 void* 命令加运行时猜测类型。命令只通过单写者的周期边界交接应用到对象，Shell 不直接写子类状态。

普通开环的 3 s 计时归 application，不能写死在模式内部。校准复用同一个开环输出算法，具有自己的行程和超时。

标定参数由 application 持有并计算电角度：

`θe = wrap(direction × pole_pairs × θm + offset)`。

编码器电压和电流模式的 enter 均要求有效标定与编码器反馈；电流模式另要求有效零偏和电流反馈。开环无须已辨识极对数，但仍要求保护采样有效。

## 4. 独立算法与统一输出链路

### 4.1 Clarke、Park、逆 Park

幅值不变变换，U/V/W 相序固定：

```text
Iw = -Iu - Iv
Iα = Iu
Iβ = (Iu + 2Iv) / √3

Id =  Iα cosθ + Iβ sinθ
Iq = -Iα sinθ + Iβ cosθ

Uα = Ud cosθ - Uq sinθ
Uβ = Ud sinθ + Uq cosθ
```

分别提供 Clarke、Park、InversePark 纯函数，Park 与逆 Park 接收已计算的 sin/cos，避免同一角度重复求值。W 为重建值，三相求和为零不能作为独立采样正确性证据。

公共执行器在编码器模式下只计算一次编码器角度的 sin/cos。开环输出使用自身角度，若不同于编码器角度，逆 Park 必须使用开环输出角度，不能误用反馈角度。

### 4.2 PI 与电流闭环

PI 独立于电机模块。采用位置式 PI，Kp 单位 V/A，Ki 单位 V/(A·s)，Kaw 单位 1/s；算法内部乘 dt，不让参数含糊地依赖采样频率。

```c
typedef struct {
    float kp, ki, kaw;
    float integral;
    float integral_min, integral_max;
} PI_Controller;

void PI_Reset(PI_Controller *self, float integral);
float PI_Step(PI_Controller *self, float error, float dt_s);
void PI_Track(PI_Controller *self, float requested,
              float applied, float dt_s);
```

执行顺序：电流指令限幅与斜坡 → Id/Iq 误差 → 两个 PI 候选电压 → dq 联合矢量限幅 → 两个 PI 回算积分。

回算项为 `Kaw × (applied - requested) × dt`，积分本身也有限幅。Track 每周期调用一次，非饱和时差值为零。不得用两个轴各自的独立输出上下限替代 dq 联合限幅。

current.step 使用本周期 voltage_limit_v 完成联合限幅和回算；公共执行器再次检查相同电压约束。正常输入下两者应一致；最终边界若发生超容差改变，按内部计算异常关断，不静默施加与 PI 认知不一致的电压。

进入、退出及故障清理积分；不做微分、解耦、反电动势补偿。PI 参数不照搬未确认的参考电机参数：允许静态配置，未配置合法参数时电流模式返回 NOT_READY。初次调试前必须记录实际 Kp/Ki/Kaw、电流指令上限和独立过流阈值，禁止凭空生成可带电默认值。

### 4.3 SVPWM

SVPWM 单独提供纯函数，输入 Uα/Uβ 和母线 V，输出逻辑 U/V/W 占空比，不接触 TIM1。

```text
Vu = Uα
Vv = -Uα/2 + √3·Uβ/2
Vw = -Uα/2 - √3·Uβ/2
Voffset = -(max(Vu,Vv,Vw) + min(Vu,Vv,Vw)) / 2
Dx = 0.5 + (Vx + Voffset) / Vbus
```

根据当前允许的占空比上下限 Dmin、Dmax 和实时有效母线电压，采用保守圆形电压限幅；当前零序注入以 50% 为中心，使用两侧较小裕量：

`m_available = 2 × min(0.5 - Dmin, Dmax - 0.5)`

`Ulimit = m_available × Vbus / √3`。

对于初始 5%～95% 占空比边界，得到 `Ulimit = 0.9 × Vbus / √3`。不再叠加固定 8 V 上限。每个控制周期使用最近有效且未过期的母线样本计算限幅，将同一个值传给电流 PI 联合限幅、抗积分饱和及公共输出检查。母线样本无效或过期时关断输出，不使用名义母线电压替代。

限幅对 dq 矢量等比例缩放。最终占空比只允许浮点舍入级修正；超范围、NaN/Inf、母线非法均报错停机，不分别硬裁三相来掩盖错误。采样窗口若要求更小调制范围，统一收紧 Ulimit；不能只在 porting 偷改占空比。

### 4.4 每周期顺序

1. 校验同步样本序号、转换状态和有效窗口，扣零偏并按增益/极性换算电流。
2. 检查电流和母线保护；母线使用带年龄检查的最近有效样本，不假装每周期都有新母线值。
3. 读取编码器并检查有效性；编码器模式计算标定电角度、Clarke/Park 得到 Id/Iq。
4. 接收本周期命令快照，调用当前模式 step。
5. 检查数值和电压限幅，逆 Park、SVPWM，检查占空比。
6. 写入 PWM 预装载寄存器；在规定更新事件统一生效，发布遥测快照。

任意关键检查失败，先直接关闭 PWM，再锁存首故障及快照，不继续执行本周期调制。

## 5. 中断如何传递到电机控制

### 5.1 实际调用链

```text
TIM1 硬件事件触发 ADC1/ADC2 注入同步转换
  → ADC 完成 IRQ（porting）
  → 检查完成/溢出，读取两路结果，清标志
  → callback(callback_context, &sample)
  → App_Motor_FastStep（application，仍在 ISR）
  → 反馈/保护/FOC_Step/逆 Park/SVPWM
  → 已绑定的 PWM 能力
  → TIM1 CCR 预装载
  → 约定更新事件生效
```

选择一个主完成中断源；不能 ADC1 和 ADC2 分别触发一次电流环。确认双 ADC 完成时序后读取成对结果。TIM1 的重复计数、触发边沿、ADC 完成与预装载生效点必须形成时间图并实测。

这是同步函数调用，不经任务通知、队列或调度器。50 μs 是周期总预算，不是全部可供计算的时间；完成转换到寄存器更新截止之间的可用时间更短，必须测量。

### 5.2 跨层回调类型

```c
typedef struct {
    uint16_t current_u_raw;
    uint16_t current_v_raw;
    uint32_t sequence;
    uint32_t flags;          /* 成对完成、溢出等有效性信息 */
} MotorSample;

typedef void (*MotorSampleCallback)(
    void *context, const MotorSample *sample);
```

类型放 common/motor_io.h，common 只定义、不实现。应用必须检查 flags 后才能使用样本。sample 为本次同步调用的借用对象，回调不得保存指针供任务延后使用。

porting 的静态采样上下文保存 callback 和 callback_context；不 include application 头，也不引用 App_Motor_FastStep 符号。

### 5.3 BSP bind 与注册

硬件能力至少提供 init、start/stop_sampling、enable/disable_output、write_duty、set_sample_callback，全部使用不透明 context。write_duty 接收逻辑 U/V/W，由 porting 根据 BSP 传入的配置映射到 CH1/CH3/CH2。

```text
BSP_Motor_Bind()
  绑定板级 TIM/ADC/引脚/通道配置与静态 porting 实例
  组装 BSP_Motor 对外能力表，不启动采样或输出

App_Motor_Init()
  构造三个 FOC 模式和唯一控制执行器
  通过 BSP_Motor.set_sample_callback 绑定 App_Motor_FastStep 与 motor 上下文

main 在调度前完成硬件安全初始化；startup 构造算法、注册回调；测试时清理标志
  → 启动采样（PWM 功率输出仍关闭）
  → 反馈/零偏/启动检查通过
  → 接受显式启动请求并允许 PWM 输出
```

BSP 注册接口只转接实例绑定，不执行控制。只有采样停止且相关中断禁用时才允许更改回调；未注册不得启动采样。构造与真正硬件初始化分开，避免 Bind 意外使能功率输出。

disable_output 必须幂等、非阻塞，任务与 ISR 均可调用；对故障执行硬件关断且锁存禁止重新使能的条件，防止任务启动流程与故障中断交错后再次开启输出。

### 5.4 调度和共享数据

- 快速 IRQ 目标优先级 4，FreeRTOS 系统调用阈值 5；不调用任何 RTOS API，不打印、不 malloc、不等锁、不写 Flash。
- 新增一个电机 supervisor 任务，初始栈预算 2048 B，周期 1 ms；实际余量需验证。负责命令、状态、流程推进和日志。
- supervisor 是命令唯一写者；Shell 提交请求，不直接修改 ISR 工作对象。发布方案必须保证缓冲区读取期间不被覆盖，不能只用翻转索引冒充完整的并发协议；后续运行命令采用短临界区复制固定大小命令，屏蔽并恢复目标采样 IRQ，测量最长屏蔽时间。阶段 1 只在采样停止时设置模式参数。
- 模式角度、PI 积分、斜坡状态仅 ISR 修改。停机并确认快速控制不再访问对象后，任务才可重设参数或切换模式。
- 遥测由 ISR 单写，任务用序号验证的一致快照读取；ISR 不自旋。任务读失败可等待下周期重试。
- 编码器 SPI 控制运行时由快速环独占，test enc 返回缓存；停机且无实时占用时才允许直接诊断读寄存器。
- 现有架构“中断只搬数据/通知”需明确补充：电机同步采样回调允许执行有界实时计算；不推广到普通外设中断。

## 6. 生命周期、故障与校准

启动条件：硬件初始化完成、反馈有效、零偏有效、母线有效、无锁存故障；编码器电压和电流模式另要求标定有效。命令接受不等于已启动，以执行器状态确认。

停机先关 PWM，再处理模式退出。故障清除后保持停止，不自动恢复。reboot、dfu、assert、栈溢出和 CPU Fault 路径均须具备直接关闭功率输出的能力；异常入口不能依赖正常业务调用链或完好的任务栈。

使用现有 Sys_Status 返回调用结果、System_Fault 表示锁存类别。额外保存轻量首故障快照：模块、原因、时间、模式、关键采样及计数。故障日志由任务输出。

校准使用开环电压子类，初始电压保留 5 V，电频率 2 Hz；此值不代表已验证足以可靠拖动当前电机。先完成对齐，再按机械角度展开值判断正反各一圈。极对数搜索范围初始 1～64，结合正反行程辨识与残差验证，不能预设 14。

已知极对数 p 时单向理论时间 p/|fe|，正反共 2p/|fe|。首次未知时按支持上限给出超时，取得可信估计后收紧后续阶段；加对齐、斜坡、稳定时间及明确裕量，并另设无运动/反馈失效检测。不能用固定 80 s 替代阶段估计。质量阈值与裕量在校准实现前单独审核，未定值前不允许自动写入有效标定。

方向和零偏可吸收部分换相效果，不能仅凭编码器唯一识别物理 UVW 标号。保留已确认板级映射，另用受控激励验证电流通道/极性一致性，不自动适配任意接线。具体激励和通过阈值作为采样阶段的前置审核项。

成功校准后关闭输出、停止采样、确认无实时访问，再擦写 Flash 并读回校验。记录包含固定宽度字段、版本、长度、CRC 和最后写入的有效标记；写入地址/长度按 8 B 补齐，不要求每个字段占 8 B。2 KB 本轮独占；掉电造成记录无效时下次启动为未校准，不自动运行。禁止直接持久化带指针的运行结构体。

## 7. 最终目标修改目录

以下是本设计约定的目标清单，**不是已完成清单**。实施时逐项标记新增/修改/移除并填写实际结果；新增未列文件必须说明职责，避免收尾时无法核对。

```text
Docs/project_memory/
  12_foc_design.md                    本设计
  13_foc_stage1_validation.md          阶段 1 实际清单、命令及验证证据
  README.md                          设计索引
  04_architecture.md                  实施时同步 BSP bind 和实时回调例外
  05_platform_rtos.md                 实施时同步任务、堆和硬件时序

project/common/
  motor_io.h                         新增：跨层采样、回调及硬件能力类型
  storage_layout.h                   修改：参数记录布局常量
  system_state.h                     按需要补充现有故障位，不重复建错误体系

project/middleware/control/
  pi.h / pi.c                        新增：位置式 PI 与回算抗饱和
  ramp.h / ramp.c                    新增：标量/矢量斜坡
project/middleware/foc/
  foc_base.h / foc_base.c             新增：基类、输入输出及统一接口
  foc_open_voltage.h / .c             新增：开环电压模式
  foc_encoder_voltage.h / .c          新增：编码器电压模式
  foc_current.h / .c                  新增：dq 电流闭环
  foc_transform.h / .c                新增：Clarke/Park/逆 Park
  foc_svpwm.h / .c                    新增：独立 SVPWM
  foc_math.h / .c                     新增：角度、sin/cos 与矢量工具

project/bsp/motor/
  bsp_motor.h / bsp_motor.c           新增：静态硬件资源 bind 与对外能力
project/bsp/bsp_board_config.h        修改：PWM、同步采样的板级配置
project/bsp/encoder/                  保留薄接口，按实时占用约定补充注释

project/porting/pwm_output/           修改：TIM1 输出与紧急关断，移除仅 GPIO 占位实现
project/porting/motor_sampling/       新增：TIM1 采样触发、双 ADC 同步、母线采样、IRQ/回调
project/porting/current_sampling/    合并到 motor_sampling 后移除旧 GPIO 初始化
project/porting/adc_regular/         合并到 motor_sampling 后移除旧独立初始化
project/porting/stm32g4/             修改：板级启动、共享 IRQ 与异常关断接入
project/porting/freertos/            修改：堆预算、内核异常关断

project/application/motor/
  app_motor.h / app_motor.c           新增：执行器、模式组装、生命周期、命令交接
  app_motor_calibration.h / .c        新增：校准状态流程与在线统计
  app_motor_storage.h / .c            新增：记录序列化、CRC、保存加载
project/application/task/
  app_motor_task.c                    新增：1 ms supervisor
project/application/shell/commands/ 修改：电机命令、补全、test enc 缓存、重启/DFU 停机
project/application/state/          修改：故障与整机状态关联
project/application/main.c          修改：实际入口以现有文件为准，接入启动顺序

cmake/                              修改：对应模块构建输入
tests/                              本地算法与接口检查，遵守现有不入库约定
```

TIM1 时基与 CH4 采样触发只由 motor_sampling 的统一初始化协调；PWM 模块不得再次复位 TIM1，两个 ADC 也不得由旧 adc_regular 路径重复初始化。接口上区分资源配置与启停，防止模块各自初始化同一外设。

## 8. 实施前检查与验证要求

1. 算法：已知向量、变换往返、PI 时间单位、积分复位、联合饱和和恢复、斜坡、SVPWM 六方向/零输入/母线边界、NaN/Inf。
2. 硬件时序：按用户确认的台架条件验证 PWM 通道/极性/死区；确认一个控制周期只有一次同步回调，测量 ADC、编码器、计算和 PWM 生效时刻。
3. 采样：零偏、U/V 极性与增益、受控激励映射检查、有效采样窗口与母线样本年龄。
4. 模式：开环默认参数、未校准禁用编码器模式、停机切换、电流指令斜坡、低电流闭环方向和饱和恢复。
5. 故障：采样失效、编码器失效、过流/母线异常、计算超时、非法数值、reboot/dfu 与异常关断；清故障不自动启动。
6. 存储：空白/坏 CRC/版本不匹配记录均未校准，写后读回，保存期间输出和采样关闭。
7. 资源：Debug/Release 构建、RAM/Flash 增量、堆启动峰值、任务栈余量、快速 IRQ 最坏耗时及更新截止余量。

最终按 Debug → Release → Debug 做三轮短时实板回归，不安排长时间测试，最终验收由用户评估。不自动提交或推送。

本文件明确了基类、算法和回调接口；尚未审核的 PI/保护参数、采样窗口、标定质量阈值不伪装成已确定值，必须在对应带电阶段前补齐并记录证据。


## 审查整改实现补充（2026-11-01）

当前只实例化已接入的开环模式，编码器电压/电流模式保留纯算法，接入运行命令时再构造对象。生产停机采样由app_motor.c持有，Shell入口及Debug链路/软件自检集中app_motor_diagnostics.c。固定PWM诊断也使用运行执行器的采零、母线/电流保护及收尾；停止立即关输出，准备期采样异步由其所有者取消，硬件等待不关全局中断。最终实际清单与构建开关见17_foc_fixes.md。
