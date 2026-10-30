/**
 *  @file   bsp_internal_flash.c
 *  @brief  将片内 Flash 板级实例绑定到独立 porting 实现。
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
 *  2026/10/30 | kaouduo | V1.0 | 独立提供片内 Flash 实例，不增加逐项转发函数。
 */
#include "bsp_internal_flash.h"
#include "internal_flash.h"

const BSP_InternalFlashTypedef BSP_InternalFlash = {
        .init = InternalFlash_Init,
        .read = InternalFlash_Read,
        .erasePage = InternalFlash_ErasePage,
        .write = InternalFlash_Write,
};
