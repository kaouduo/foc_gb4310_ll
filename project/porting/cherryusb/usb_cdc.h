/**
 *  @file   usb_cdc.h
 *  @brief  提供静态 CDC 端点传输和状态接口。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / cherryusb
 *
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 接入单总线 CDC 描述符与静态异步传输。
 */
#ifndef USB_CDC_H_PORT
#define USB_CDC_H_PORT
#include <stdbool.h>
#include <stdint.h>
#include "system_status.h"
#include "usb_cdc_types.h"
#include "usb_descriptors.h"
/**
 *  @brief  注册描述符和端点并初始化 CDC 控制器。
 *  @param  config 设备身份和供电参数。
 *  @param  registerBase USB 寄存器基地址。
 *  @param  callback 有效的 IRQ 事件回调，不可阻塞。
 *  @param  context 生命周期覆盖设备运行的回调上下文，可为 NULL。
 *  @return SYS_OK 表示初始化完成；控制器失败返回 SYS_ERROR_IO。
 *  @note   任务中仅调用一次，之前完成 BSP 引脚准备；内部先注册状态与回调、开启 IRQ，再接入 D+。
 */
Sys_Status USB_CDC_Init(const USB_Descriptors_Config *config, uintptr_t registerBase, USB_CDC_EventCallback callback,
                        void *context);
/** @brief 任务中查询已配置、未挂起且传输无错误的状态。 */
bool USB_CDC_IsConfigured(void);
/**
 *  @brief  非阻塞取走待接收字节。
 *  @param  data 可写 capacity 字节的缓冲。
 *  @param  capacity 非零接收容量，允许分次取走一个包。
 *  @param  actualLength 有效输出指针，返回实际字节数。
 *  @return SYS_OK、BUSY（无数据）、NOT_READY、IO 或 PARAM（容量为零）。
 *  @note   仅单个任务调用；失败时长度为 0，若重新挂接失败则当前包作废，等待重新配置。
 */
Sys_Status USB_CDC_Read(uint8_t *data, uint32_t capacity, uint32_t *actualLength);
/**
 *  @brief  复制并异步发送一个数据块。
 *  @param  data 有效的 length 字节数据，返回后可复用。
 *  @param  length 1–64 字节。
 *  @return SYS_OK 表示接受；BUSY、NOT_READY、IO 或 PARAM 表示未接受。
 *  @note   仅单个任务调用；TX_DONE 包含必要 ZLP，USB 复位会取消旧发送而不报告完成。
 */
Sys_Status USB_CDC_Write(const uint8_t *data, uint32_t length);
/** @brief USB IRQ 尾部同步取消配置；不从任务或其他中断调用。 */
void USB_CDC_AfterIRQ(void);
/** @brief IRQ 尾部传入 FSDEV 挂起状态，补齐 v1.6.1 未上报的 suspend/resume 事件。 */
void USB_CDC_SyncSuspend(bool suspended);
/** @brief 任务内停止 CDC、断开上拉并关闭 IRQ；之后不再调用收发。 */
void USB_CDC_Deinit(void);
/** @brief 当前总线会话代次；复位/取消配置/重新配置递增，供单任务丢弃旧文本。 */
uint32_t USB_CDC_SessionId(void);
#endif
