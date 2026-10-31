/**
 *  @file   usb_descriptors.c
 *  @brief  实现 Full Speed CDC 描述符和稳定序列号。
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
#include "usb_descriptors.h"
#include "usbd_core.h"

/* 上游类头依赖 core 提供的打包宏和类型。 */
#include "usbd_cdc_acm.h"

static uint8_t deviceDescriptor[] = {USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, 0, 0, 0x0100, 0x01)};
static uint8_t configurationDescriptor[] = {
        USB_CONFIG_DESCRIPTOR_INIT(9 + CDC_ACM_DESCRIPTOR_LEN, 2, 1, USB_CONFIG_BUS_POWERED, 100),
        CDC_ACM_DESCRIPTOR_INIT(0, 0x83, 0x02, 0x81, 64, 5)};
static char serialNumber[25];
static const char languageId[] = {0x09, 0x04};
static const char *strings[] = {languageId, NULL, NULL, serialNumber, "CDC Config", "CDC Interface"};

static const uint8_t *DeviceDescriptor(uint8_t speed)
{
    (void)speed;
    return deviceDescriptor;
}
static const uint8_t *ConfigurationDescriptor(uint8_t speed)
{
    (void)speed;
    return configurationDescriptor;
}
static const char *StringDescriptor(uint8_t speed, uint8_t index)
{
    (void)speed;
    return index < sizeof(strings) / sizeof(strings[0]) ? strings[index] : NULL;
}
static const struct usb_descriptor descriptors = {
        .device_descriptor_callback = DeviceDescriptor,
        .config_descriptor_callback = ConfigurationDescriptor,
        .string_descriptor_callback = StringDescriptor,
};

void USB_Descriptors_Register(const USB_Descriptors_Config *config)
{
    deviceDescriptor[8] = (uint8_t)config->vid;
    deviceDescriptor[9] = (uint8_t)(config->vid >> 8);
    deviceDescriptor[10] = (uint8_t)config->pid;
    deviceDescriptor[11] = (uint8_t)(config->pid >> 8);
    configurationDescriptor[6] = 4U;
    configurationDescriptor[7] = config->selfPowered ? USB_CONFIG_SELF_POWERED : USB_CONFIG_BUS_POWERED;
    configurationDescriptor[8] = (uint8_t)(config->maxPowerMa / 2U);
    memcpy(serialNumber, config->serial, 24U);
    serialNumber[24] = '\0';
    strings[1] = config->manufacturer;
    strings[2] = config->product;
    usbd_desc_register(0U, &descriptors);
}
