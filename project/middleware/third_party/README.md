# 外来代码

此目录保存原厂源码，不手改、不纳入工程 clang-format。适配配置放在 `project/porting/`。

| 组件 | 版本与来源 |
|---|---|
| CMSIS | STM32CubeG4 V1.6.3，来自本机 CubeMX 官方固件包 |
| STM32G4xx_LL_Driver | STM32CubeG4 V1.6.3，仅保留全部 LL 源码、头文件及许可证 |
| FreeRTOS-Kernel | 官方稳定标签 V11.3.2，2026-10-16 发布，MIT 许可证 |
| CherryUSB | 官方稳定标签 v1.6.1，Apache-2.0 许可证；阶段 3 已接通 CDC 设备初始化，实板未验证 |
| CherrySH | 固定 c8c1c0964bc0033f24efb255cd607b35e3a19490，Apache-2.0；S0 独立归档，未链接固件 |
| EasyLogger | 固定 806328e131836662fa83dd43364a53f699fd76ac，MIT；S0 独立归档，未链接固件 |

FreeRTOS 官方源码：[FreeRTOS/FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/V11.3.2)。
获取地址：`https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/zip/refs/tags/V11.3.2`。
SHA256：`d840bdc153ad08e1fc5c915ea71643b58173fa33129c3b390e4ab1de3bee65f6`。

FreeRTOS 源码目录已加入 `.gitignore`，缺失时 CMake 自动下载并验证固定归档的哈希。
配置与源码清单位于 `cmake/freertos.cmake`，修改版本时必须同时更新 URL、哈希与版本检查。
已有完整源码后可离线构建。首次联网获取发生在 CMake 配置阶段，不在设备运行时发生。

## CherrySH 与 EasyLogger（2026-10-31，S0）

本阶段只生成独立静态库，不链接固件、不初始化、不输出日志；不是 Shell/日志功能已完成。

| 项目 | 固定提交 | 归档 SHA256 |
|---|---|---|
| CherrySH | `c8c1c0964bc0033f24efb255cd607b35e3a19490` | `45316d2517dfc470461c08141b5578954a6131d56a61d30c9ac4d8c65a833e93` |
| EasyLogger | `806328e131836662fa83dd43364a53f699fd76ac` | `8836303efe2a90afe3081fbedb106129f63c7092206cf0995e81f8ff8cc82fe0` |

官方来源：[CherrySH](https://github.com/cherry-embedded/CherrySH)、[EasyLogger](https://github.com/armink/EasyLogger)。URL 固定为 `https://codeload.github.com/<owner>/<repo>/zip/<完整commit>`。

完整源码下载到当前目录的 CherrySH/、EasyLogger/，不入库、不改动。CMake 首次配置缺失时自动获取并校验归档；已有目录只核对清单，不自动覆盖。已有完整源码后支持 `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`，源码缺失明确报错。

构建与校验：cmake/cherrysh.cmake、cherrysh.sha256（9 项），cmake/easylogger.cmake、easylogger.sha256（4 项）。升级时同步更新完整 commit、归档哈希和输入清单。

- CherrySH：配置在 project/porting/cherrysh/csh_config.h；仅编译 chry_shell.c、cherryrl/chry_readline.c。上游 help 在 S2 直接编入固件；不编译 clear/login/示例。Newlib 的 strnlen 声明由库目标私有 `_POSIX_C_SOURCE=200809L` 启用。
- EasyLogger：配置在 project/porting/easylogger/elog_cfg.h；仅编译 elog.c、elog_utils.c。不编译 elog_async/elog_buf 或插件，不启用 pthread。Debug 目标公开 FOC_LOG_DEBUG_BUILD 定义，使未来调用侧与库内等级保持一致；Release 无此定义。

当前统一语言标准 C23/C++23。详细版本、构建、负向检查和阶段目录见 [计划](../../../Docs/project_memory/10_shell_log_plan.md) 与 [验收](../../../Docs/project_memory/11_shell_log_validation.md)。

## CherryUSB

官方发布：[CherryUSB v1.6.1](https://github.com/cherry-embedded/CherryUSB/releases/tag/v1.6.1)。

- commit：`c9625ffa773ad10b8824d1b5361bca2ccc1f3d1e`。
- 获取地址：`https://codeload.github.com/cherry-embedded/CherryUSB/zip/refs/tags/v1.6.1`。
- 归档 SHA256：`04862e5ffcbacc5747bc1b0fa068d58be1e307761bd2076e78c6ceaeda9f028b`。
- 源码目录：`project/middleware/third_party/CherryUSB/`，不入库，不修改、不格式化上游内容。
- 配置和构建：`cmake/cherryusb.cmake`、`project/porting/cherryusb/usb_config.h`。
- `cmake/cherryusb.sha256` 保存本次构建源码、头文件及许可证的 23 项校验值；已有目录也检查版本和哈希，不静默覆盖错误源码。

只编译 `core/usbd_core.c`、`class/cdc/usbd_cdc_acm.c`、`port/fsdev/usb_dc_fsdev.c`，输出 `cherryusb_device` 静态库。保持完整上游目录用于阅读，但不编译 Host、其他类、示例或 HAL 胶水。阶段 1 仅构建库，阶段 2 接通中断但不启动设备栈；阶段 3 在 BSP 启动时序后注册描述符、调用 usbd_initialize 并开启 USB IRQ。回显与命令任务仍待阶段 4，实板 CDC 枚举尚未核验。

首次 `cmake --preset REV6-Debug` 缺少源码目录时自动获取并校验归档。已有源码后可用 `cmake --preset REV6-Debug -DFETCHCONTENT_FULLY_DISCONNECTED=ON` 验证不获取依赖的配置路径，然后正常构建。恢复自动获取可设该变量为 `OFF`。离线模式下源码缺失明确报错。

错误版本、文件缺失或哈希变化均使配置失败；先保存本地改动、核对清单，不在错误目录上自动重新下载覆盖。升级版本时同步更新 tag、commit、归档哈希和文件清单。

上游 FSDEV 控制器代码保留在第三方目录，虽然直接操作寄存器，也不搬迁至项目 porting。项目使用 `usb_dc_port.c` 的 LL 胶水，不引入 `usb_glue_st.c`。当前配置不打印、保留上游断言、不启用 EP0 线程；PMA 访问值 1 与现有 G4 LL 一致，容量已按 G431 数据手册显式设为 1024 B。

阶段 3 的传输与描述符适配位于 `usb_cdc.c/.h`、`usb_descriptors.c/.h`。v1.6.1 的 SET_CONFIGURATION(0) 不发送取消配置事件，项目在 LP IRQ 尾部同步并关闭数据端点，不修改上游核心。Full Speed 设备不提供高速 qualifier/other-speed 描述符，相关请求由核心按不支持处理。
