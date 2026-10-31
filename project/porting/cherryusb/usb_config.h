/**
 *  @file   usb_config.h
 *  @brief  配置单总线 Full Speed CherryUSB 设备栈。
 *  @author kaouduo
 *  @date   2026/10/31
 *  @version V1.0
 *
 *  @par 文件所属
 *  porting / cherryusb
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/31 | kaouduo | V1.0 | 建立 CDC 与 FSDEV 最小编译配置，暂不启动硬件。
 */

#ifndef USB_CONFIG_H
#define USB_CONFIG_H

/* 关闭包括原始输出在内的日志，保留上游断言停机行为。 */
#define CONFIG_USB_DBG_LEVEL (-1)
#define CONFIG_USB_PRINTF(...) ((void)0)

#define CONFIG_USBDEV_MAX_BUS 1
#define CONFIG_USB_ALIGN_SIZE 4
/* G431 无数据缓存，使用默认 RAM 段，不增加链接脚本段。 */
#define USB_NOCACHE_RAM_SECTION
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 256
#define CONFIG_USBDEV_ADVANCE_DESC

/* G431 的 USB 专用包内存为 1 KiB，16 位数据连续映射。 */
#define CONFIG_USBDEV_FSDEV_PMA_ACCESS 1
#define CONFIG_USB_FSDEV_RAM_SIZE 1024

/* 不定义 USB_HS、EP0_THREAD、DCACHE、SOF 和其他设备类的线程开关。 */

#endif /* USB_CONFIG_H */
