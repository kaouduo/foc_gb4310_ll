# FreeRTOS CLion 调试（任务视图）

更新日期：2026-10-31（Asia/Taipei）。记录本次对话关于 CLion RTOS 视图排查收敛的结论，不逐条记录尝试过程。

## 结论

- CLion 的 FreeRTOS 插件分两步：**检测**（`detect()` / `get_config()`）只读 ELF 的 DWARF 类型（`TCB_t`、`List_t`…）与全局符号（`pxCurrentTCB`），不读芯片内存；**任务列表**（`get_freertos_tasks()` / `get_curr_task()`）才读目标内存里的 `pxCurrentTCB`、`pxReadyTasksLists`、`xDelayedTaskList1/2` 等链表。
- “任务列表为空、但没有配置告警”是正常现象：配置本身正确（检测通过），只是暂停那一刻没有任何任务在这些链表里——通常是没打断点、目标在跑，或停在 `xTaskCreateStatic` 之前的 `main` 开头。
- 必须使用 CLion 自带 GDB（支持 Python）；`arm-none-eabi-gdb` 无 Python，RTOS 标签不出现。Release（`-g0`）无调试符号，检测不到 FreeRTOS。

## 推荐断点位置

- `main()` 中 `vTaskStartScheduler()` 之后：`startup`(Running) + `IDLE` + `Tmr Svc`。
- `App_LEDTask`（10 ms 周期任务，`project/application/task/app_led_task.c`）：`led`(Running) + `IDLE` + `Tmr Svc`，最稳定，必被命中。
- `main` 最开头（`xTaskCreateStatic` 之前）：空列表，属正常，不是故障。

## 视图字段

- 状态：`Running` / `Ready` / `Blocked` / `Delayed` / `Suspended`；优先级 `led`=1、`startup`=2、`IDLE`=0、`Tmr Svc`=1。
- `stackHWMark`：验证 `LED_STACK_BYTES=2048` 等栈余量的直接手段。
- `runtime` 占比：来自 `configGENERATE_RUN_TIME_STATS=1` + DWT 运行统计，回绕约 71.6 分钟，后续接入 FOC 需实测开销。
- 双击任务可切换调试上下文，查看该任务的调用栈与寄存器，用于排查“任务卡死在哪”。

## 离线检查边界

`tests/rtos_debug_check.gdb` 只验证 `detect()` + `get_config()`（类型、符号、配置标志），**不验证任务枚举**；离线通过 ≠ 板上任务视图已确认。实际显示仍需真实调试会话：打断点、让目标停在任务已创建的代码段后查看。
