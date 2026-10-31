/**
 *  @file   usb_cdc_types.h
 *  @brief  定义跨 BSP 与平台适配共用的 CDC 事件。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *
 *  @par 文件所属
 *  common
 *
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 接入单总线 CDC 描述符与静态异步传输。
 */
#ifndef USB_CDC_TYPES_H
#define USB_CDC_TYPES_H
#include <stdint.h>
/** @brief CDC 中断通知位，可组合；状态通知后由任务查询最新连接状态。 */
typedef enum {
    USB_CDC_EVENT_RX_READY = 1U << 0, /*!< 接收数据等待任务取走。 */
    USB_CDC_EVENT_TX_DONE = 1U << 1,  /*!< 数据及必要 ZLP 发送完成。 */
    USB_CDC_EVENT_STATE = 1U << 2,    /*!< 配置、复位、挂起或恢复发生变化。 */
    USB_CDC_EVENT_ERROR = 1U << 3     /*!< 传输提交失败，等待主机重新配置。 */
} USB_CDC_Event;
/** @brief IRQ 上下文回调；只允许记录事件和合规的 FromISR 操作，不阻塞。 */
typedef void (*USB_CDC_EventCallback)(uint32_t events, void *context);
#endif
