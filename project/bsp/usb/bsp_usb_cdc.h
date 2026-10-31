/**
 *  @file   bsp_usb_cdc.h
 *  @brief  提供 USB CDC 板级启动准备接口。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *
 *  @par 文件所属
 *  BSP / usb
 *
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 组织复位及数据线准备，不启动设备栈。
 */
#ifndef BSP_USB_CDC_H
#define BSP_USB_CDC_H

#include <stdbool.h>
#include <stdint.h>
#include "system_status.h"
#include "usb_cdc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  完成保持 USB 断开的板级重新枚举准备。
 *  @param  无。
 *  @return 无返回值。
 *  @note   平台时钟和调度器启动后，在启动设备栈前或停止 CDC 后交接 ROM 前调用；
 *          阻塞至少约 500 ms，保持 IRQ 关闭、控制器时钟门控开启，不枚举 CDC。
 */
void BSP_USB_CDC_Prepare(void);

/**
 *  @brief  初始化板级 CDC 设备并允许主机枚举。
 *  @param  callback 有效的 IRQ 回调，只允许记录状态及合规 FromISR 操作。
 *  @param  context 生命周期覆盖设备运行的回调上下文，可为 NULL。
 *  @return SYS_OK 表示控制器启动；失败透传 IO 错误，不等待主机连接。
 *  @note   平台和调度器启动后在启动任务调用一次，内部包含 Prepare，不可提前重复调用 Prepare。
 */
Sys_Status BSP_USB_CDC_Init(USB_CDC_EventCallback callback, void *context);

/**
 *  @brief  查询 CDC 当前是否允许传输。
 *  @param  无。
 *  @return 已配置、未挂起且无传输错误时为 true。
 *  @note   初始化后在任务中调用；不是检测 USB 供电或主机串口打开状态。
 */
bool BSP_USB_CDC_IsConfigured(void);
/** @brief CDC 可传输且终端 DTR 打开，或已收到 OUT 数据；任务中查询。 */
bool BSP_USB_CDC_IsTerminalOpen(void);

/**
 *  @brief  非阻塞读取 CDC 已接收的数据。
 *  @param  data 有效的 capacity 字节可写空间。
 *  @param  capacity 非零容量，允许分次读取一个包。
 *  @param  actualLength 有效的输出指针，返回取走长度。
 *  @return SYS_OK；无数据 BUSY；未连接 NOT_READY；重挂接失败 IO；容量为零 PARAM。
 *  @note   仅单个任务调用；失败长度为 0，重挂接失败作废当前包并等待重新配置。
 */
Sys_Status BSP_USB_CDC_Read(uint8_t *data, uint32_t capacity, uint32_t *actualLength);

/**
 *  @brief  将数据复制到内部缓冲并异步发送。
 *  @param  data 有效的 length 字节数据，返回后可复用。
 *  @param  length 1–64 字节，更长数据由任务分块。
 *  @return SYS_OK 表示接受；忙 BUSY；未就绪 NOT_READY；提交失败 IO；长度越界 PARAM。
 *  @note   仅单个任务调用，等待 TX_DONE 再发下一块；复位可取消传输，成功不代表主机应用已读取。
 */
Sys_Status BSP_USB_CDC_Write(const uint8_t *data, uint32_t length);

/** @brief 任务内永久停止当前 CDC 会话，供平台进入出厂 DFU 前断开 USB。 */
void BSP_USB_CDC_Deinit(void);
/** @brief 获取总线会话代次，复位及重新配置时改变；不代表 VBUS 电平。 */
uint32_t BSP_USB_CDC_SessionId(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_USB_CDC_H */
