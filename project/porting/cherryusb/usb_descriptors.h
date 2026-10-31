/**
 *  @file   usb_descriptors.h
 *  @brief  定义由 BSP 下传的 USB 身份和供电描述参数。
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
#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H
#include <stdbool.h>
#include <stdint.h>
/** @brief 固定设备描述参数；字符串来源须在初始化期间有效。 */
typedef struct {
    uint16_t vid;             /*!< BSP 提供的本地验证 VID。 */
    uint16_t pid;             /*!< BSP 提供的本地验证 PID。 */
    uint16_t maxPowerMa;      /*!< USB 取电声明，0–500 mA，偶数。 */
    bool selfPowered;         /*!< 配置是否声明自供电。 */
    const char *manufacturer; /*!< 生命周期覆盖设备运行的制造商字符串。 */
    const char *product;      /*!< 生命周期覆盖设备运行的产品字符串。 */
    const char *serial;       /*!< 24 位十六进制 UID 字符串，本模块复制保存。 */
} USB_Descriptors_Config;
/**
 *  @brief  填充并注册单路 CDC 描述符。
 *  @param  config BSP 提供的有效固定参数。
 *  @return 无返回值。
 *  @note   仅启动时、USB IRQ 关闭且设备未连接时调用；不读取 UID、不启动硬件。
 */
void USB_Descriptors_Register(const USB_Descriptors_Config *config);
#endif
