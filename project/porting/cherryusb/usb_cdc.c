/**
 *  @file   usb_cdc.c
 *  @brief  实现 CDC 静态收发、背压及会话状态同步。
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
#include "usb_cdc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usbd_core.h"

/* 上游类头依赖 core 提供的打包宏和类型。 */
#include "usbd_cdc_acm.h"

#define CDC_BUS 0U
#define CDC_IN 0x81U
#define CDC_OUT 0x02U
#define CDC_NOTIFY 0x83U
#define CDC_PACKET_BYTES 64U

typedef struct {
    volatile bool configured;
    volatile bool terminalOpen;
    volatile bool suspended;
    volatile bool fault;
    volatile bool rxReady;
    volatile bool txBusy;
    volatile uint32_t rxLength;
    volatile uint32_t rxOffset;
} CDC_State;

static CDC_State state;
static volatile uint32_t sessionId;
static USB_MEM_ALIGNX uint8_t rxBuffer[CDC_PACKET_BYTES];
static USB_MEM_ALIGNX uint8_t txBuffer[CDC_PACKET_BYTES];
static USB_CDC_EventCallback eventCallback;
static void *eventContext;
static struct usbd_interface communicationInterface;
static struct usbd_interface dataInterface;
static struct cdc_line_coding lineCoding;

static void ClearSession(void)
{
    state = (CDC_State){0};
    ++sessionId;
}

uint32_t USB_CDC_SessionId(void) { return sessionId; }

static Sys_Status StartRead(void)
{
    if (usbd_ep_start_read(CDC_BUS, CDC_OUT, rxBuffer, sizeof(rxBuffer)) != 0) {
        state.fault = true;
        return SYS_ERROR_IO;
    }
    return SYS_OK;
}

static Sys_Status TransferStatus(void)
{
    if (!state.configured || !usb_device_is_configured(CDC_BUS) || state.suspended) {
        return SYS_ERROR_NOT_READY;
    }
    return state.fault ? SYS_ERROR_IO : SYS_OK;
}

static void ReceiveComplete(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    if (TransferStatus() != SYS_OK) {
        return;
    }
    if (nbytes == 0U) {
        if (StartRead() != SYS_OK) {
            eventCallback(USB_CDC_EVENT_ERROR, eventContext);
        }
        return;
    }
    /* 对不切换 DTR 的终端，实际 OUT 数据也视为连接就绪。 */
    state.terminalOpen = true;
    state.rxOffset = 0U;
    state.rxLength = nbytes;
    state.rxReady = true;
    /* 保留 NAK，直到任务取完当前包，避免覆盖尚未处理的数据。 */
    eventCallback(USB_CDC_EVENT_RX_READY, eventContext);
}

static void TransmitComplete(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    if (!state.txBusy || !state.configured || state.fault || !usb_device_is_configured(CDC_BUS)) {
        return;
    }
    if (nbytes == CDC_PACKET_BYTES) {
        if (usbd_ep_start_write(CDC_BUS, CDC_IN, NULL, 0U) != 0) {
            state.txBusy = false;
            state.fault = true;
            eventCallback(USB_CDC_EVENT_ERROR, eventContext);
        }
        return;
    }
    state.txBusy = false;
    eventCallback(USB_CDC_EVENT_TX_DONE, eventContext);
}

static struct usbd_endpoint receiveEndpoint = {.ep_addr = CDC_OUT, .ep_cb = ReceiveComplete};
static struct usbd_endpoint transmitEndpoint = {.ep_addr = CDC_IN, .ep_cb = TransmitComplete};

static void DeviceEvent(uint8_t busid, uint8_t event)
{
    (void)busid;
    switch (event) {
        case USBD_EVENT_CONFIGURED:
            ClearSession();
            state.configured = true;
            if (StartRead() != SYS_OK) {
                eventCallback(USB_CDC_EVENT_STATE | USB_CDC_EVENT_ERROR, eventContext);
            } else {
                eventCallback(USB_CDC_EVENT_STATE, eventContext);
            }
            break;
        case USBD_EVENT_RESET:
        case USBD_EVENT_DISCONNECTED:
            ClearSession();
            eventCallback(USB_CDC_EVENT_STATE, eventContext);
            break;
        case USBD_EVENT_SUSPEND:
            state.suspended = true;
            eventCallback(USB_CDC_EVENT_STATE, eventContext);
            break;
        case USBD_EVENT_RESUME:
            state.suspended = false;
            eventCallback(USB_CDC_EVENT_STATE, eventContext);
            break;
        case USBD_EVENT_ERROR:
            state.fault = true;
            eventCallback(USB_CDC_EVENT_ERROR, eventContext);
            break;
        default:
            /* INIT/DEINIT 来自任务，不通过仅限 IRQ 的回调向上通知。 */
            break;
    }
}

Sys_Status USB_CDC_Init(const USB_Descriptors_Config *config, uintptr_t registerBase, USB_CDC_EventCallback callback,
                        void *context)
{
    ClearSession();
    eventCallback = callback;
    eventContext = context;
    lineCoding = (struct cdc_line_coding){.dwDTERate = 115200U, .bDataBits = 8U};
    USB_Descriptors_Register(config);
    usbd_add_interface(CDC_BUS, usbd_cdc_acm_init_intf(CDC_BUS, &communicationInterface));
    usbd_add_interface(CDC_BUS, usbd_cdc_acm_init_intf(CDC_BUS, &dataInterface));
    usbd_add_endpoint(CDC_BUS, &receiveEndpoint);
    usbd_add_endpoint(CDC_BUS, &transmitEndpoint);
    if (usbd_initialize(CDC_BUS, registerBase, DeviceEvent) != 0) {
        (void)usbd_deinitialize(CDC_BUS);
        return SYS_ERROR_IO;
    }
    return SYS_OK;
}

void USB_CDC_Deinit(void)
{
    taskENTER_CRITICAL();
    (void)usbd_deinitialize(CDC_BUS);
    ClearSession();
    taskEXIT_CRITICAL();
}

bool USB_CDC_IsConfigured(void)
{
    taskENTER_CRITICAL();
    bool configured = TransferStatus() == SYS_OK;
    taskEXIT_CRITICAL();
    return configured;
}

Sys_Status USB_CDC_Read(uint8_t *data, uint32_t capacity, uint32_t *actualLength)
{
    *actualLength = 0U;
    if (capacity == 0U) {
        return SYS_ERROR_PARAM;
    }
    taskENTER_CRITICAL();
    Sys_Status result = TransferStatus();
    if (result == SYS_OK && !state.rxReady) {
        result = SYS_ERROR_BUSY;
    }
    if (result == SYS_OK) {
        uint32_t remaining = state.rxLength - state.rxOffset;
        uint32_t length = capacity < remaining ? capacity : remaining;
        memcpy(data, rxBuffer + state.rxOffset, length);
        state.rxOffset += length;
        if (state.rxOffset == state.rxLength) {
            state.rxReady = false;
            result = StartRead();
        }
        if (result == SYS_OK) {
            *actualLength = length;
        }
    }
    taskEXIT_CRITICAL();
    return result;
}

Sys_Status USB_CDC_Write(const uint8_t *data, uint32_t length)
{
    if (length == 0U || length > sizeof(txBuffer)) {
        return SYS_ERROR_PARAM;
    }
    taskENTER_CRITICAL();
    Sys_Status result = TransferStatus();
    if (result == SYS_OK && state.txBusy) {
        result = SYS_ERROR_BUSY;
    }
    if (result == SYS_OK) {
        memcpy(txBuffer, data, length);
        state.txBusy = true;
        if (usbd_ep_start_write(CDC_BUS, CDC_IN, txBuffer, length) != 0) {
            state.txBusy = false;
            state.fault = true;
            result = SYS_ERROR_IO;
        }
    }
    taskEXIT_CRITICAL();
    return result;
}

void USB_CDC_SyncSuspend(bool suspended)
{
    if (state.configured && state.suspended != suspended) {
        DeviceEvent(CDC_BUS, suspended ? USBD_EVENT_SUSPEND : USBD_EVENT_RESUME);
    }
}

void USB_CDC_AfterIRQ(void)
{
    /* 上游 v1.6.1 对 SET_CONFIGURATION(0) 不发事件，在同一个 IRQ 尾部补齐。 */
    if (state.configured && !usb_device_is_configured(CDC_BUS)) {
        (void)usbd_ep_close(CDC_BUS, CDC_IN);
        (void)usbd_ep_close(CDC_BUS, CDC_OUT);
        (void)usbd_ep_close(CDC_BUS, CDC_NOTIFY);
        ClearSession();
        eventCallback(USB_CDC_EVENT_STATE, eventContext);
    }
}

/** @brief 保存主机的串口配置；不改变 USB 时钟或传输速率。 */
void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *coding)
{
    (void)busid;
    (void)intf;
    memcpy(&lineCoding, coding, sizeof(lineCoding));
}

/** @brief 返回本次运行中保存的串口配置。 */
void usbd_cdc_acm_get_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *coding)
{
    (void)busid;
    (void)intf;
    memcpy(coding, &lineCoding, sizeof(lineCoding));
}

/* 类请求来自 USB IRQ；只发布状态，不在这里等待或发送。 */
void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    if (busid != CDC_BUS || intf != 0U) {
        return;
    }
    state.terminalOpen = dtr;
    eventCallback(USB_CDC_EVENT_STATE, eventContext);
}

bool USB_CDC_IsTerminalOpen(void)
{
    taskENTER_CRITICAL();
    bool ready = TransferStatus() == SYS_OK && state.terminalOpen;
    taskEXIT_CRITICAL();
    return ready;
}
