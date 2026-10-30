# 外来代码

此目录保存原厂源码，不手改、不纳入工程 clang-format。适配配置放在 `project/porting/`。

| 组件 | 版本与来源 |
|---|---|
| CMSIS | STM32CubeG4 V1.6.3，来自本机 CubeMX 官方固件包 |
| STM32G4xx_LL_Driver | STM32CubeG4 V1.6.3，仅保留全部 LL 源码、头文件及许可证 |
| FreeRTOS-Kernel | 官方稳定标签 V11.3.2，2026-10-16 发布，MIT 许可证 |

FreeRTOS 官方源码：[FreeRTOS/FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/V11.3.2)。
获取地址：`https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/zip/refs/tags/V11.3.2`。
SHA256：`d840bdc153ad08e1fc5c915ea71643b58173fa33129c3b390e4ab1de3bee65f6`。

FreeRTOS 源码目录已加入 `.gitignore`，缺失时 CMake 自动下载并验证固定归档的哈希。
配置与源码清单位于 `cmake/freertos.cmake`，修改版本时必须同时更新 URL、哈希与版本检查。
已有完整源码后可离线构建。首次联网获取发生在 CMake 配置阶段，不在设备运行时发生。
