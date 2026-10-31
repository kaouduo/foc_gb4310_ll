/**
 *  @file   usb_dc_port.h
 *  @brief  定义 USB 控制器复位准备与中断启用接口。
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
 *  2026/10/31 | kaouduo | V1.0 | 分离控制器生命周期与 GPIO 操作。
 */
#ifndef USB_DC_PORT_H
#define USB_DC_PORT_H

#include <stdint.h>

/**
 *  @brief  关闭 USB 中断并保持控制器复位。
 *  @param  priority BSP 指定的抢占优先级，使用 RTOS 时须满足系统调用门限。
 *  @return 无返回值。
 *  @note   在设备栈启动前，或停止设备栈后交接 ROM 前调用；不修改系统时钟源，不处理延时。
 */
void USB_DC_BeginReset(uint32_t priority);

/**
 *  @brief  释放外设复位并保持控制器断开、掉电。
 *  @param  无。
 *  @return 无返回值。
 *  @note   BSP 完成复位保持时间后调用，USB IRQ 仍关闭；之后可操作数据线。
 */
void USB_DC_EndReset(void);

#endif /* USB_DC_PORT_H */
