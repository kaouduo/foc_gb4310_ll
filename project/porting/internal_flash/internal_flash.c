/**
 *  @file   internal_flash.c
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

#include "internal_flash.h"
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "stm32g4xx.h"
#include "storage_layout.h"

#define FLASH_ERROR_MASK                                                                                               \
    (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR |       \
     FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR | FLASH_SR_OPTVERR)
#define FLASH_TIMEOUT_US 100000U

static StaticSemaphore_t flashMutexControl;
static SemaphoreHandle_t flashMutex;

/**
 *  @brief  创建 Flash 操作使用的静态互斥量。
 *  @param  无。
 *  @return SYS_OK 表示静态互斥量已建立。
 *  @note   调度器运行后的启动任务中调用一次；调用方保证只初始化一次；不分配动态堆，不解锁或擦写 Flash。
 */
Sys_Status InternalFlash_Init(void)
{
    flashMutex = xSemaphoreCreateMutexStatic(&flashMutexControl);
    vQueueAddToRegistry(flashMutex, "internal_flash");
    return SYS_OK;
}

/**
 *  @brief  零等待获取 Flash 静态互斥量。
 *  @param  无。
 *  @return SYS_OK 表示已持有互斥量；SYS_ERROR_BUSY 表示其他任务正在使用。
 *  @note   Flash 初始化后仅任务调用；获取成功后由外层接口释放，失败不改变存储内容。
 */
static Sys_Status AcquireFlash(void) { return xSemaphoreTake(flashMutex, 0U) == pdTRUE ? SYS_OK : SYS_ERROR_BUSY; }

/**
 *  @brief  检查非空地址区间是否完整落在指定存储范围内。
 *  @param  address 待检查的绝对起始地址。
 *  @param  size 待检查的字节数，要求非零。
 *  @param  start 允许范围的起始地址。
 *  @param  length 允许范围的字节数。
 *  @return 1 表示整个区间有效，0 表示为空或越界。
 *  @note   仅进行整数比较，避免地址加法溢出；不读取目标存储。
 */
static int RangeValid(uint32_t address, uint32_t size, uint32_t start, uint32_t length)
{
    return size != 0U && address >= start && size <= length && address - start <= length - size;
}

/**
 *  @brief  等待 Flash 操作结束并清除完成及错误标志。
 *  @param  无。
 *  @return SYS_OK 表示完成且无错误；SYS_ERROR_TIMEOUT 表示等待超时；SYS_ERROR_IO 表示硬件报错。
 *  @note   DWT 已运行且主频固定；超时保留硬件标志，其他返回会清标志，失败不保证目标内容。
 */
static Sys_Status WaitComplete(void)
{
    uint32_t begin = DWT->CYCCNT;
    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        if ((uint32_t)(DWT->CYCCNT - begin) >= (SystemCoreClock / 1000000U) * FLASH_TIMEOUT_US) {
            return SYS_ERROR_TIMEOUT;
        }
    }
    uint32_t errors = FLASH->SR & FLASH_ERROR_MASK;
    FLASH->SR = FLASH_ERROR_MASK | FLASH_SR_EOP;
    return errors == 0U ? SYS_OK : SYS_ERROR_IO;
}

/**
 *  @brief  复位 Flash 指令和数据缓存并恢复使能状态。
 *  @param  enabled 操作前保存的 FLASH_ACR_ICEN、FLASH_ACR_DCEN 位组合。
 *  @return 无返回值。
 *  @note   Flash 操作后的临界区内调用；执行同步屏障，避免继续使用擦写前的缓存内容。
 */
static void FlushCaches(uint32_t enabled)
{
    CLEAR_BIT(FLASH->ACR, FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    SET_BIT(FLASH->ACR, FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    CLEAR_BIT(FLASH->ACR, FLASH_ACR_ICRST | FLASH_ACR_DCRST);
    SET_BIT(FLASH->ACR, enabled);
    __DSB();
    __ISB();
}

/**
 *  @brief  检查 Flash 空闲状态并解锁操作寄存器。
 *  @param  无。
 *  @return SYS_OK 表示已解锁并清除旧标志；SYS_ERROR_BUSY 表示操作未结束；SYS_ERROR_IO 表示解锁失败。
 *  @note   调用方已持有互斥量；成功后负责结束操作并重新上锁，此处不擦写数据。
 */
static Sys_Status BeginOperation(void)
{
    if ((FLASH->SR & FLASH_SR_BSY) != 0U ||
        (FLASH->CR & (FLASH_CR_PG | FLASH_CR_PER | FLASH_CR_MER1 | FLASH_CR_FSTPG)) != 0U) {
        return SYS_ERROR_BUSY;
    }
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        return SYS_ERROR_IO;
    }
    FLASH->SR = FLASH_ERROR_MASK | FLASH_SR_EOP;
    return SYS_OK;
}

/**
 *  @brief  读取已验证范围内的主 Flash 数据。
 *  @param  address 主 Flash 内的绝对地址。
 *  @param  buffer 容量不少于 size 的有效可写 RAM。
 *  @param  size 非零字节数，完整范围须在主 Flash 内。
 *  @return SYS_OK 表示复制完成；SYS_ERROR_PARAM 表示范围无效；SYS_ERROR_BUSY 表示硬件忙。
 *  @note   调用方持有 Flash 互斥量；返回错误时不修改输出，不恢复 ECC 异常。
 */
static Sys_Status FlashRead(uint32_t address, void *buffer, uint32_t size)
{
    if (!RangeValid(address, size, STORAGE_FLASH_START, STORAGE_FLASH_SIZE)) {
        return SYS_ERROR_PARAM;
    }
    if ((FLASH->SR & FLASH_SR_BSY) != 0U) {
        return SYS_ERROR_BUSY;
    }
    memcpy(buffer, (const void *)address, size);
    return SYS_OK;
}

/**
 *  @brief  擦除并校验预留参数页。
 *  @param  address 参数区内按页对齐的绝对地址。
 *  @return SYS_OK 表示整页已擦除；其他返回表示参数、忙、超时或硬件校验错误。
 *  @note   调用方持有互斥量且电机已停止；操作期间屏蔽中断并恢复缓存，失败后页内容不确定。
 */
static Sys_Status FlashErasePage(uint32_t address)
{
    if (!RangeValid(address, STORAGE_FLASH_PAGE_SIZE, STORAGE_PARAMETER_START, STORAGE_PARAMETER_SIZE) ||
        (address % STORAGE_FLASH_PAGE_SIZE) != 0U) {
        return SYS_ERROR_PARAM;
    }
    Sys_Status result = BeginOperation();
    if (result != SYS_OK) {
        return result;
    }
    uint32_t page = (address - STORAGE_FLASH_START) / STORAGE_FLASH_PAGE_SIZE;
    /* 同 bank 擦写会暂停从 Flash 取指；必须在电机停止后操作。 */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t cache = FLASH->ACR & (FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    CLEAR_BIT(FLASH->ACR, FLASH_ACR_DCEN);
    MODIFY_REG(FLASH->CR, FLASH_CR_PNB, page << FLASH_CR_PNB_Pos);
    SET_BIT(FLASH->CR, FLASH_CR_PER);
    SET_BIT(FLASH->CR, FLASH_CR_STRT);
    result = WaitComplete();
    CLEAR_BIT(FLASH->CR, FLASH_CR_PER | FLASH_CR_PNB);
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);
    FlushCaches(cache);
    __set_PRIMASK(mask);
    if (result == SYS_OK) {
        for (uint32_t offset = 0U; offset < STORAGE_FLASH_PAGE_SIZE; offset += 4U) {
            if (*(volatile const uint32_t *)(address + offset) != UINT32_MAX) {
                return SYS_ERROR_IO;
            }
        }
    }
    return result;
}

/**
 *  @brief  按双字写入并校验预留参数区。
 *  @param  address 参数区内 8 字节对齐的绝对地址。
 *  @param  data 操作期间稳定且不与目标重叠的数据源，可不对齐。
 *  @param  size 非零且为 8 的倍数，整个区间须在参数区内。
 *  @return SYS_OK 表示写入并验证完成；目标未擦除返回 SYS_ERROR_NOT_ERASED，其他错误透传。
 *  @note   调用方持有互斥量且电机已停止；不自动擦除，跳过全 1 双字，硬件失败可能已部分写入。
 */
static Sys_Status FlashWrite(uint32_t address, const void *data, uint32_t size)
{
    if ((address % 8U) != 0U || (size % 8U) != 0U ||
        !RangeValid(address, size, STORAGE_PARAMETER_START, STORAGE_PARAMETER_SIZE)) {
        return SYS_ERROR_PARAM;
    }
    /* 提前检查整个目标，避免因后续地址未擦除而产生可预知的部分写入。 */
    for (uint32_t offset = 0U; offset < size; offset += 4U) {
        if (*(volatile const uint32_t *)(address + offset) != UINT32_MAX) {
            return SYS_ERROR_NOT_ERASED;
        }
    }
    Sys_Status result = BeginOperation();
    if (result != SYS_OK) {
        return result;
    }
    for (uint32_t offset = 0U; offset < size; offset += 8U) {
        uint32_t words[2];
        memcpy(words, (const uint8_t *)data + offset, sizeof(words));
        /* 跳过全 1 双字，避免消耗 ECC 编程状态却仍呈现为擦除数据。 */
        if (words[0] == UINT32_MAX && words[1] == UINT32_MAX) {
            continue;
        }
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        /* 按原厂擦写流程暂时关闭数据缓存，完成后复位并恢复缓存状态。 */
        uint32_t cache = FLASH->ACR & (FLASH_ACR_ICEN | FLASH_ACR_DCEN);
        CLEAR_BIT(FLASH->ACR, FLASH_ACR_DCEN);
        SET_BIT(FLASH->CR, FLASH_CR_PG);
        *(volatile uint32_t *)(address + offset) = words[0];
        __ISB();
        *(volatile uint32_t *)(address + offset + 4U) = words[1];
        result = WaitComplete();
        CLEAR_BIT(FLASH->CR, FLASH_CR_PG);
        FlushCaches(cache);
        __set_PRIMASK(mask);
        if (result != SYS_OK || *(volatile const uint32_t *)(address + offset) != words[0] ||
            *(volatile const uint32_t *)(address + offset + 4U) != words[1]) {
            result = result == SYS_OK ? SYS_ERROR_IO : result;
            break;
        }
    }
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);
    return result;
}

/**
 *  @brief  在任务中读取内部 Flash 数据。
 *  @param  address 主 Flash 范围内的绝对地址。
 *  @param buffer 容量不小于 size 的有效可写 RAM 缓冲区。
 *  @param size 非零字节数，整个范围须位于 128 KB 主 Flash 内。
 *  @return SYS_OK 表示复制完成；地址范围错误或资源忙时返回对应负值。
 *  @note   仅任务调用，互斥量零等待获取，忙时立即返回；失败时不更新目标，ECC 硬件异常不在此恢复。
 */
Sys_Status InternalFlash_Read(uint32_t address, void *buffer, uint32_t size)
{
    Sys_Status result = AcquireFlash();
    if (result == SYS_OK) {
        result = FlashRead(address, buffer, size);
        (void)xSemaphoreGive(flashMutex);
    }
    return result;
}

/**
 *  @brief  在任务中擦除预留参数页。
 *  @param  address 页首绝对地址，目前只允许 0x0801F800，页长 2048 字节。
 *  @return SYS_OK 表示擦除及读回验证完成；其他负值表示地址或对齐错误、资源忙、超时或硬件错误。
 *  @note   电机停止后在普通任务中调用；静态互斥量串行化，擦除会暂停同 bank
 * 取指并暂时屏蔽中断；失败后页内容不确定，禁止在控制环运行中使用。
 */
Sys_Status InternalFlash_ErasePage(uint32_t address)
{
    Sys_Status result = AcquireFlash();
    if (result == SYS_OK) {
        result = FlashErasePage(address);
        (void)xSemaphoreGive(flashMutex);
    }
    return result;
}

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
Sys_Status InternalFlash_Write(uint32_t address, const void *data, uint32_t size)
{
    Sys_Status result = AcquireFlash();
    if (result == SYS_OK) {
        result = FlashWrite(address, data, size);
        (void)xSemaphoreGive(flashMutex);
    }
    return result;
}
