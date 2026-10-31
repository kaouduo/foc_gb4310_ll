/**
 *  @file   usb_dc_port.c
 *  @brief  适配 G431 FSDEV 复位、低层钩子和 USB 中断。
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
 *  2026/10/31 | kaouduo | V1.0 | 提供 LL 控制器生命周期，暂不启动设备栈。
 */
#include "usb_dc_port.h"
#include "dwt.h"
#include "stm32g4xx.h"
#include "stm32g4xx_ll_bus.h"
#include "usb_cdc.h"
#include "usbd_core.h"

static void DisableInterrupts(void)
{
    NVIC_DisableIRQ(USB_LP_IRQn);
    NVIC_DisableIRQ(USB_HP_IRQn);
    NVIC_DisableIRQ(USBWakeUp_IRQn);
}

static void ClearPendingInterrupts(void)
{
    NVIC_ClearPendingIRQ(USB_LP_IRQn);
    NVIC_ClearPendingIRQ(USB_HP_IRQn);
    NVIC_ClearPendingIRQ(USBWakeUp_IRQn);
}

void USB_DC_BeginReset(uint32_t priority)
{
    DisableInterrupts();
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USB);
    CLEAR_BIT(USB->BCDR, USB_BCDR_DPPU);
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_USB);
    NVIC_SetPriority(USB_LP_IRQn, priority);
}

void USB_DC_EndReset(void)
{
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_USB);
    USB->BCDR = 0U;
    USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    USB->ISTR = 0U;
    ClearPendingInterrupts();
}

/**
 *  @brief  为 CherryUSB 控制器初始化退出 PHY 掉电状态。
 *  @param  无。
 *  @return 无返回值。
 *  @note   v1.6.1 的无参数强钩子；描述符及事件已注册，驱动随后设置中断源并最后开启 D+ 上拉。
 */
void usb_dc_low_level_init(void)
{
    USB->CNTR = USB_CNTR_FRES;
    (void)DWT_DelayUs(10U);
    /* 必须先具备 IRQ 响应条件，再由上游 usb_dc_init 最后接入 D+。 */
    ClearPendingInterrupts();
    NVIC_EnableIRQ(USB_LP_IRQn);
    __DSB();
    __ISB();
}

/**
 *  @brief  在 CherryUSB 关闭后撤销连接并关闭控制器时钟。
 *  @param  无。
 *  @return 无返回值。
 *  @note   普通任务内由设备栈反初始化调用，不关闭系统 HSI48 或 CRS。
 */
void usb_dc_low_level_deinit(void)
{
    DisableInterrupts();
    CLEAR_BIT(USB->BCDR, USB_BCDR_DPPU);
    USB->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;
    USB->ISTR = 0U;
    ClearPendingInterrupts();
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_USB);
}

/** @brief 处理单总线 FSDEV 中断；回调仅允许非阻塞处理和满足优先级限制的 FromISR 操作。 */
void USB_LP_IRQHandler(void)
{
    USBD_IRQHandler(0U);
    USB_CDC_SyncSuspend((USB->CNTR & USB_CNTR_FSUSP) != 0U);
    USB_CDC_AfterIRQ();
}
