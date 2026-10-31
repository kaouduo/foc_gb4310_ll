/**
 *  @file   usb_io.h
 *  @brief  定义 USB 数据线的板级引脚配置与操作接口。
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
 *  2026/10/31 | kaouduo | V1.0 | 提供数据线拉低和高阻恢复，不配置系统时钟。
 */
#ifndef USB_IO_H
#define USB_IO_H

#include "stm32g4xx.h"

/** @brief BSP 下传的同端口 USB 数据线配置。 */
typedef struct {
    GPIO_TypeDef *port; /*!< D- 和 D+ 所在端口。 */
    uint32_t pins;      /*!< 两个 LL_GPIO_PIN_x 的组合。 */
    uint32_t clock;     /*!< 对应 AHB2 GPIO 时钟掩码。 */
} USB_IO_Config;

/**
 *  @brief  将数据线配置为输出低。
 *  @param  config BSP 提供的有效引脚配置。
 *  @return 无返回值。
 *  @note   仅在 USB 未连接且控制器停机时调用；先预置输出低再切换模式，不处理延时。
 */
void USB_IO_DriveLow(const USB_IO_Config *config);

/**
 *  @brief  将数据线恢复为无上下拉输入。
 *  @param  config 已完成拉低准备的引脚配置。
 *  @return 无返回值。
 *  @note   控制器连接前调用，释放 GPIO 驱动，由 USB 附加功能接管，不设置 AF 编号。
 */
void USB_IO_Release(const USB_IO_Config *config);

#endif /* USB_IO_H */
