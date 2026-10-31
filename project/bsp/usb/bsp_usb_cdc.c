/**
 *  @file   bsp_usb_cdc.c
 *  @brief  绑定 USB 板级参数、平台 UID 和 CDC 传输。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.1
 *
 *  @par 文件所属
 *  BSP / usb
 *
 *  @par 修改记录
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.1 | 通过平台 UID 注册描述符并接通非阻塞 CDC 传输。
 *  2026/10/31 | kaouduo | V1.0 | 编排复位 200 ms 与引脚拉低 300 ms。
 */
#include "bsp_usb_cdc.h"
#include "bsp_board_config.h"
#include "bsp_platform.h"
#include "usb_cdc.h"
#include "usb_dc_port.h"
#include "usb_io.h"

_Static_assert(BSP_BOARD_USB_MAX_POWER_MA <= 500U && BSP_BOARD_USB_MAX_POWER_MA % 2U == 0U,
               "USB power declaration must fit USB 2.0 descriptor units");

static const USB_IO_Config usbPins = {
        .port = BSP_BOARD_USB_PORT,
        .pins = BSP_BOARD_USB_DM_PIN | BSP_BOARD_USB_DP_PIN,
        .clock = BSP_BOARD_USB_GPIO_CLOCK,
};

void BSP_USB_CDC_Prepare(void)
{
    USB_DC_BeginReset(BSP_BOARD_USB_IRQ_PRIORITY);
    (void)BSP_Platform.delayMs(BSP_BOARD_USB_RESET_HOLD_MS);
    USB_DC_EndReset();
    USB_IO_DriveLow(&usbPins);
    (void)BSP_Platform.delayMs(BSP_BOARD_USB_LINE_LOW_MS);
    USB_IO_Release(&usbPins);
}

Sys_Status BSP_USB_CDC_Init(USB_CDC_EventCallback callback, void *context)
{
    BSP_USB_CDC_Prepare();
    uint32_t uid[3];
    char serial[25];
    static const char hex[] = "0123456789ABCDEF";
    (void)BSP_Platform_UniqueId(uid);
    for (uint32_t word = 0U; word < 3U; ++word) {
        for (uint32_t digit = 0U; digit < 8U; ++digit) {
            serial[word * 8U + digit] = hex[(uid[word] >> (28U - digit * 4U)) & 0xFU];
        }
    }
    serial[24] = '\0';
    const USB_Descriptors_Config descriptors = {
            .vid = BSP_BOARD_USB_VID,
            .pid = BSP_BOARD_USB_PID,
            .maxPowerMa = BSP_BOARD_USB_MAX_POWER_MA,
            .selfPowered = BSP_BOARD_USB_SELF_POWERED != 0U,
            .manufacturer = BSP_BOARD_USB_MANUFACTURER,
            .product = BSP_BOARD_USB_PRODUCT,
            .serial = serial,
    };
    return USB_CDC_Init(&descriptors, USB_BASE, callback, context);
}

bool BSP_USB_CDC_IsConfigured(void) { return USB_CDC_IsConfigured(); }
Sys_Status BSP_USB_CDC_Read(uint8_t *data, uint32_t capacity, uint32_t *actualLength)
{
    return USB_CDC_Read(data, capacity, actualLength);
}
Sys_Status BSP_USB_CDC_Write(const uint8_t *data, uint32_t length) { return USB_CDC_Write(data, length); }

void BSP_USB_CDC_Deinit(void) { USB_CDC_Deinit(); }

uint32_t BSP_USB_CDC_SessionId(void) { return USB_CDC_SessionId(); }
