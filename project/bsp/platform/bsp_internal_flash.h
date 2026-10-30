/**
 *  @file   bsp_internal_flash.h
 *  @brief  声明独立的片内 Flash 板级实例。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.0
 *
 *  @par 文件所属
 *  BSP / platform / 片内 Flash
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.0 | 独立绑定片内 Flash，不占用系统时间和测量结构。
 */
#ifndef BSP_INTERNAL_FLASH_H
#define BSP_INTERNAL_FLASH_H

#include <stdint.h>
#include "system_status.h"

/* 对外保持 C 链接，允许 C++ 业务直接包含并调用。 */
#ifdef __cplusplus
extern "C" {
#endif

/** @brief 片内 Flash 外设能力，板级静态绑定，运行中不可替换。 */
typedef struct {
    Sys_Status (*init)(void); /*!< 启动任务初始化静态互斥量一次；不擦写 Flash，返回 SYS_OK。 */
    Sys_Status (*read)(uint32_t address, void *buffer,
                       uint32_t size);         /*!< 读取主 Flash 至有效 RAM，长度为非零字节数。 */
    Sys_Status (*erasePage)(uint32_t address); /*!< 擦除 2 KB 参数页，页首地址只允许 0x0801F800。 */
    Sys_Status (*write)(uint32_t address, const void *data,
                        uint32_t size); /*!< 参数区双字编程，地址和长度均为 8 字节对齐。 */
} BSP_InternalFlashTypedef;

/**
 * @brief 供业务使用的只读片内 Flash 实例。
 * @note 先在启动任务调用 init，再在普通任务中读写；调用方保证有效缓冲区和稳定数据源。
 *       不检查调用上下文或重复初始化；互斥竞争返回 SYS_ERROR_BUSY，地址和对齐错误返回 SYS_ERROR_PARAM。
 *       擦写前必须停止电机，操作会屏蔽中断并暂停同 bank 取指；失败可能留下部分内容。
 */
extern const BSP_InternalFlashTypedef BSP_InternalFlash;

#ifdef __cplusplus
}
#endif

#endif
