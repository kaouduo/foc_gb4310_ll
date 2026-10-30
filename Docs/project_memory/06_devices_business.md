# 设备状态、LED 与 MT6826S

更新日期：2026-10-31。以当日源码为准；上板结论以用户实测为准。

## 两种不同的数据

`Sys_Status` 是**一次函数调用的结果**（成功/忙/超时/参数错误…），不表示设备当前状态。
`System_DeviceState`（`common/system_state.h`）是**持续设备状态**，实际值由 `application/state/app_device_state` 原子保存，直到启动流程或后续电机业务显式更新。
LED 任务只读 `App_DeviceState_Get()`，不读某次返回值，也不推测电机状态。

| 持久状态 | 含义 | 红灯 |
|---|---|---|
| INITIALIZING | 从复位开始初始化平台、设备与业务 | 不启动灯效任务，驱动初始化后保持熄灭 |
| STANDBY | 资源就绪、输出关闭、等待控制请求 | 2 秒一轮呼吸 |
| CALIBRATING | 正在执行电机或编码器标定 | 100 ms 亮 / 100 ms 灭 / 100 ms 亮 / 700 ms 灭 |
| RUNNING | 电机控制正在执行 | 常亮 |
| FAULT | 存在锁存故障 | 100 ms 亮 / 100 ms 灭 |

开环/CW/电流/速度/位置等控制模式由后续 FOC 维护；RUNNING 不等于转子一定有转速（闭环位置保持也是 RUNNING）。
目前未实现 FOC，成功启动后进入 STANDBY，不会虚报 RUNNING 或 CALIBRATING。

## 发布与恢复规则

- `App_DeviceState_Set` 只发布非 FAULT 状态；故障必须走 `App_DeviceState_RaiseFault`，保留具体原因位。
- 存在任何故障时普通状态发布不能覆盖 FAULT；故障位不会因初始化完成或停止而丢失。
- 多个来源的故障可同时锁存，`ClearFault` 清指定位置不影响其他故障；最后一个故障清除后回到 STANDBY，**不自动恢复 RUNNING**。
- 故障恢复只由上层管理者在故障源静止或确认恢复后执行；LED 任务绝不清故障。
- 设备状态发布不是启停命令：FOC 必须先完成真正的使能/关断/标定转换，再发布对应状态。
- 并发模型：一个无锁 32 位原子字保存基础状态（低 8 位）与故障位（上 24 位），一次读取得到一致快照；存在故障时读出 state 为 FAULT。`RaiseFault` 可在任务或普通中断调用，不调用 RTOS。
- 原子状态字不替代 FOC 采样数据同步、实际保护关断或共享内存发布协议。

## LED 分层

- `driver/led/led_base`：LED 基类与 `init / on / off / setBrightness` 虚函数表。
- `porting/led/led_gpio`、`led_pwm`：LED 作为结构首成员，保存各自硬件配置并实现虚函数。
- `bsp/led`：私有保存红灯 PWM 对象与绿灯 GPIO 对象，对外只暴露 `LED *BSP_LED_Red`、`*BSP_LED_Green`。
- `application/led/app_led.c`：状态到灯效映射；`app_led_task.c` 每 10 ms 调 `LED_SetBrightness(BSP_LED_Red, brightness)`。
- `LED_PinConfig` 共享引脚配置类型在 `porting/led/led_pin.h`；板级实例与具体参数由 BSP 提供。

`BSP_LED_Bind` 只绑定配置与操作表，不写寄存器；`LED_Init` 由子类完整初始化硬件（GPIO 子类开端口时钟、配置熄灭电平与普通输出；PWM 子类配置 GPIO、TIM4，输出就绪后切 PB6 复用）。
红灯 PB6 AF2/TIM4_CH1：TIM4 由 LED 独占，PSC=169、ARR=999 → 1 kHz PWM，比较值 0 灭、1000 全亮。模式变化重置灯效计时，快闪从亮相开始。
不逐脉冲翻转 GPIO，不占用 FOC 的 TIM1。LED 任务独占红灯输出，其他业务只通过状态影响灯效。
基础时钟失败、异常关中断或调度器停止时无法保证指示，灯效不能当作安全关断机制。

## MT6826S 磁编码器

分层：`driver/mt6826s` 负责命令、CRC、角度解码、方向配置，只依赖自身头与 `common/system_status.h`；`porting/mt6826s` 接收板级参数实现 LL SPI、CS、完整事务互斥与 DWT 超时，不引用 BSP；`bsp/encoder` 私有持有器件与端点，对外 `BSP_Encoder_Init` / `Read` / `ReadRegister`，全部返回 `Sys_Status`。

- SPI1：PA5/PA6/PA7 AF5，CS 为 PC4；模式 3、8 位、MSB 优先、PCLK2/32（当前 5.3125 MHz）。
- 读角度：单次 CS 下发送 `A0 03 00 00 00 00`，后四字节为角度高、角度低、状态、CRC；角度 0–32767；CRC-8 多项式 0x07、初值 0、MSB first，覆盖三个数据字节。
- 初始化：配置硬件并绑定对象，等 5 ms 上电余量，读 `0x00D`、仅改 bit3 ROT_DIR 为 1（CW，顺时针递增），按需发送 `60 0D data` 后整字节读回校验；其他位保留。
- 方向按芯片顶面观察定义；不烧 EEPROM，每次上电重配。
- 初始化不读取角度、不判断磁场，应用当前也**不调用**读取接口；不添加周期采样、速度估算、电角度换算、20 kHz 中断或 shell。
- MOSI 预置沿用参考板：CS 高电平先发 0，再拉低 CS 执行实际命令；CS 建立/保持/间隔各留 1 us。
- 原子标志保护完整事务，竞争立即返回 BUSY，不使用 RTOS 锁或屏蔽中断；所有等待共享 30 us DWT 预算，出错释放 CS 并复位专用 SPI。
- `ReadRegister` 用于诊断寄存器，单寄存器应答无 CRC，不用于分步读取角度锁存区。

错误码沿用统一 `Sys_Status`：超时 -2、IO -4、忙 -5、CRC -7、帧/固定字段 -8、芯片状态 -9；读取失败不覆盖调用方原有角度。
全零帧可能通过 CRC，通信返回成功不能单独证明器件在线。
`BSP_Encoder_Read` 预留给后续 20 kHz 控制或 shell；当前零等待互斥不等于已证明满足 20 kHz 时限，收尾、恢复与抢占都会增加总耗时，接入前需实测并协调 shell 访问归属。

## 验证状态

已覆盖并确认：Debug/Release 构建、主机测试（协议向量、逐位损坏、方向位保留、失败路径、启动顺序、状态与灯效映射、DWT 计数回绕、tick 回绕）。
用户反馈硬件已实测；仍未实现的是 FOC 算法、TIM1 完整控制、PWM 使能与死区、硬件保护、ADC 同步采样、20 kHz 控制中断、shell。
