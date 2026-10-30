/**
 *  @file   internal_flash.h
 *  @brief  提供任务互斥的内部 Flash 参数存储接口。
 *  @author kaouduo
 *  @date   2026/10/30
 *  @version V1.1
 *
 *  @par 文件所属
 *  porting / internal_flash
 *
 *  @par 修改记录
 *
 *  日期 | 作者 | 版本 | 说明
 *  --- | --- | --- | ---
 *  2026/10/30 | kaouduo | V1.1 | 接入 FreeRTOS，将 DWT 测量与内核系统时间分开。
 */

#ifndef INTERNAL_FLASH_H
#define INTERNAL_FLASH_H

#include <stdint.h>
#include "system_status.h"

/**
 *  @brief  创建 Flash 操作使用的静态互斥量。
 *  @param  无。
 *  @return SYS_OK 表示静态互斥量已建立。
 *  @note   调度器运行后的启动任务中调用一次；调用方保证只初始化一次；不分配动态堆，不解锁或擦写 Flash。
 */
Sys_Status InternalFlash_Init(void);

/**
 *  @brief  在任务中读取内部 Flash 数据。
 *  @param  address 主 Flash 范围内的绝对地址。
 *  @param buffer 容量不小于 size 的有效可写 RAM 缓冲区。
 *  @param size 非零字节数，整个范围须位于 128 KB 主 Flash 内。
 *  @return SYS_OK 表示复制完成；地址范围错误或资源忙时返回对应负值。
 *  @note   仅任务调用，互斥量零等待获取，忙时立即返回；失败时不更新目标，ECC 硬件异常不在此恢复。
 */
Sys_Status InternalFlash_Read(uint32_t address, void *buffer, uint32_t size);
/**
 *  @brief  在任务中擦除预留参数页。
 *  @param  address 页首绝对地址，目前只允许 0x0801F800，页长 2048 字节。
 *  @return SYS_OK 表示擦除及读回验证完成；其他负值表示地址或对齐错误、资源忙、超时或硬件错误。
 *  @note   电机停止后在普通任务中调用；静态互斥量串行化，擦除会暂停同 bank
 * 取指并暂时屏蔽中断；失败后页内容不确定，禁止在控制环运行中使用。
 */
Sys_Status InternalFlash_ErasePage(uint32_t address);
/**
 *  @brief  在任务中按双字编程参数区。
 *  @param  address 参数区内 8 字节对齐的绝对地址。
 *  @param data 操作期间稳定的有效数据源，不得与目标重叠，可不对齐。
 *  @param size 非零且为 8 的倍数，整个范围须位于预留 2 KB 参数区。
 *  @return SYS_OK 表示完成并验证；SYS_ERROR_NOT_ERASED
 * 表示目标未擦除；其他负值表示地址或对齐错误、资源忙、超时或硬件错误。
 *  @note   电机停止后在普通任务中调用；不自动擦除，全 1
 * 双字跳过；互斥量零等待获取，失败可能部分写入，调用者须处理记录校验和掉电恢复。
 */
Sys_Status InternalFlash_Write(uint32_t address, const void *data, uint32_t size);

#endif
