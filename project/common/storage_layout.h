/**
 *  @file   storage_layout.h
 *  @brief  定义主 Flash 与持久化参数区的地址布局。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  common
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 实现内部 HSI 最高主频、系统计时及受限 Flash 参数存储。
 */

#ifndef STORAGE_LAYOUT_H
#define STORAGE_LAYOUT_H /* 存储布局头文件保护。 */

/* STM32G431CB 的最后一页保留为参数区，链接脚本必须同步保留。 */
#define STORAGE_FLASH_START 0x08000000UL               /* 主 Flash 起始绝对地址。 */
#define STORAGE_FLASH_SIZE (128UL * 1024UL)            /* 主 Flash 总容量，单位字节。 */
#define STORAGE_FLASH_PAGE_SIZE 2048UL                 /* 单页擦除长度为 2048 字节。 */
#define STORAGE_PARAMETER_START 0x0801F800UL           /* 预留参数页的起始绝对地址。 */
#define STORAGE_PARAMETER_SIZE STORAGE_FLASH_PAGE_SIZE /* 参数区占一页，长度为 2048 字节。 */

#endif
