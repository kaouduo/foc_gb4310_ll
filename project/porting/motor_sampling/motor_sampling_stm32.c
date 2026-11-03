/**
 * @file motor_sampling_stm32.c
 * @brief TIM1 CH4 触发双 ADC，主 ADC IRQ 同步调用控制回调。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include "motor_sampling_stm32.h"
#include <limits.h>
#include "dwt.h"
#include "pwm_output_stm32.h"
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_tim.h"
static MotorSTM32 *irqMotor;
/** @brief 轮询等待指定位清零，最多 1000 μs。 */
static Sys_Status WaitClear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t start = DWT_Cycles();
    while (*reg & mask) {
        if (DWT_ElapsedUs(start) >= 1000U) {
            return SYS_ERROR_TIMEOUT;
        }
    }
    return SYS_OK;
}
/** @brief 绑定 TIM1/ADC1/ADC2 专用实例及 IRQ 目标。 */
void MotorSTM32_Bind(MotorSTM32 *self, const MotorSTM32Config *config)
{
    *self = (MotorSTM32){.config = config};
    irqMotor = self;
}
/** @brief 设置注入通道，校准 ADC 并等待就绪。 */
static Sys_Status InitADC(ADC_TypeDef *adc, uint32_t channel)
{
    LL_ADC_DisableDeepPowerDown(adc);
    LL_ADC_EnableInternalRegulator(adc);
    (void)DWT_DelayUs(25U);
    LL_ADC_SetChannelSingleDiff(adc, channel, LL_ADC_SINGLE_ENDED);
    LL_ADC_StartCalibration(adc, LL_ADC_SINGLE_ENDED);
    Sys_Status status = WaitClear(&adc->CR, ADC_CR_ADCAL);
    if (status != SYS_OK) {
        return status;
    }
    (void)DWT_DelayUs(2U);
    LL_ADC_INJ_SetQueueMode(adc, LL_ADC_INJ_QUEUE_DISABLE);
    LL_ADC_INJ_SetSequencerLength(adc, LL_ADC_INJ_SEQ_SCAN_DISABLE);
    LL_ADC_INJ_SetSequencerRanks(adc, LL_ADC_INJ_RANK_1, channel);
    LL_ADC_SetChannelSamplingTime(adc, channel, LL_ADC_SAMPLINGTIME_12CYCLES_5);
    LL_ADC_INJ_SetTriggerSource(adc, LL_ADC_INJ_TRIG_EXT_TIM1_TRGO2);
    LL_ADC_INJ_SetTriggerEdge(adc, LL_ADC_INJ_TRIG_EXT_RISING);
    LL_ADC_ClearFlag_ADRDY(adc);
    LL_ADC_Enable(adc);
    uint32_t start = DWT_Cycles();
    while (!LL_ADC_IsActiveFlag_ADRDY(adc)) {
        if (DWT_ElapsedUs(start) >= 1000U) {
            return SYS_ERROR_TIMEOUT;
        }
    }
    return SYS_OK;
}
/** @brief 初始化安全 PWM 引脚、TIM1 及双 ADC。 */
Sys_Status MotorSTM32_Init(void *context)
{
    MotorSTM32 *self = context;
    const MotorSTM32Config *cfg = self->config;
    LL_AHB2_GRP1_EnableClock(cfg->gpio_clocks | LL_AHB2_GRP1_PERIPH_ADC12);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
    LL_APB2_GRP1_ForceReset(LL_APB2_GRP1_PERIPH_TIM1);
    LL_APB2_GRP1_ReleaseReset(LL_APB2_GRP1_PERIPH_TIM1);
    PWM_Output_EmergencyOff();
    for (unsigned i = 0; i < 6U; ++i) {
        const MotorPin *pin = &cfg->pwm[i];
        LL_GPIO_SetPinOutputType(pin->port, pin->pin, LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinSpeed(pin->port, pin->pin, LL_GPIO_SPEED_FREQ_HIGH);
        LL_GPIO_SetPinPull(pin->port, pin->pin, LL_GPIO_PULL_DOWN);
        if (pin->pin <= LL_GPIO_PIN_7) {
            LL_GPIO_SetAFPin_0_7(pin->port, pin->pin, pin->alternate);
        } else {
            LL_GPIO_SetAFPin_8_15(pin->port, pin->pin, pin->alternate);
        }
        LL_GPIO_SetPinMode(pin->port, pin->pin, LL_GPIO_MODE_ALTERNATE);
    }
    for (unsigned i = 0; i < 3U; ++i) {
        LL_GPIO_SetPinPull(cfg->analog[i].port, cfg->analog[i].pin, LL_GPIO_PULL_NO);
        LL_GPIO_SetPinMode(cfg->analog[i].port, cfg->analog[i].pin, LL_GPIO_MODE_ANALOG);
    }
    uint32_t arr = cfg->timer_hz / (2U * cfg->pwm_hz);
    /* 完整周期 2*ARR；CH4 PWM2 的上数沿在每周期仅触发一次。 */
    TIM1->PSC = 0U;
    TIM1->ARR = arr;
    TIM1->RCR = 1U;
    TIM1->CR1 = TIM_CR1_CMS_0 | TIM_CR1_ARPE;
    TIM1->CCMR1 = (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
    TIM1->CCMR2 = (6U << TIM_CCMR2_OC3M_Pos) | TIM_CCMR2_OC3PE | (7U << TIM_CCMR2_OC4M_Pos) | TIM_CCMR2_OC4PE;
    TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 = arr / 2U;
    TIM1->CCR4 = arr - 80U;
    LL_TIM_SetTriggerOutput2(TIM1, LL_TIM_TRGO2_OC4);
    uint32_t dead = (uint32_t)(((uint64_t)cfg->timer_hz * cfg->deadtime_ns + 999999999ULL) / 1000000000ULL);
    if (dead > 127U) {
        return SYS_ERROR_PARAM;
    }
    TIM1->BDTR = dead | TIM_BDTR_OSSI | TIM_BDTR_OSSR;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    LL_AHB2_GRP1_ForceReset(LL_AHB2_GRP1_PERIPH_ADC12);
    LL_AHB2_GRP1_ReleaseReset(LL_AHB2_GRP1_PERIPH_ADC12);
    LL_ADC_SetCommonClock(ADC12_COMMON, LL_ADC_CLOCK_SYNC_PCLK_DIV4);
    LL_ADC_SetMultimode(ADC12_COMMON, LL_ADC_MULTI_DUAL_INJ_SIMULT);
    Sys_Status status = InitADC(ADC2, cfg->slave_channel);
    if (status == SYS_OK) {
        status = InitADC(ADC1, cfg->master_channel);
    }
    if (status != SYS_OK) {
        PWM_Output_Disable(self);
        return status;
    }
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, cfg->bus_channel);
    LL_ADC_SetChannelSamplingTime(ADC1, cfg->bus_channel, LL_ADC_SAMPLINGTIME_247CYCLES_5);
    NVIC_SetPriority(ADC1_2_IRQn, cfg->irq_priority);
    NVIC_SetPriority(TIM1_UP_TIM16_IRQn, cfg->irq_priority);
    DBGMCU->APB2FZ |= DBGMCU_APB2FZ_DBG_TIM1_STOP;
    self->initialized = true;
    return SYS_OK;
}
/** @brief 在采样停止时注册同步 IRQ 回调。 */
Sys_Status MotorSTM32_SetCallback(void *context, MotorSampleCallback callback, void *user)
{
    MotorSTM32 *self = context;
    if (self->sampling) {
        return SYS_ERROR_BUSY;
    }
    self->callback = callback;
    self->callback_context = user;
    return SYS_OK;
}
/** @brief 清除统计并启动 TIM1 触发双 ADC 采样。 */
Sys_Status MotorSTM32_Start(void *context)
{
    MotorSTM32 *self = context;
    if (self->sampling || (TIM1->BDTR & TIM_BDTR_MOE)) {
        return SYS_ERROR_BUSY;
    }
    if (!self->initialized || !self->callback) {
        return SYS_ERROR_NOT_READY;
    }
    self->samples = self->errors = self->irq_max = self->interval_max = self->lease_periods = 0U;
    self->adc_errors = self->deadline_errors = 0U;
    self->deadline_min = UINT32_MAX;
    self->interval_min = UINT32_MAX;
    self->previous_cycles = 0U;
    ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC | ADC_ISR_JQOVF | ADC_ISR_OVR;
    ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC | ADC_ISR_JQOVF | ADC_ISR_OVR;
    NVIC_ClearPendingIRQ(ADC1_2_IRQn);
    NVIC_ClearPendingIRQ(TIM1_UP_TIM16_IRQn);
    TIM1->CNT = 0U;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    /* 同步模式只启动主 ADC，从 ADC 只使能。 */
    self->sampling = true;
    ADC1->IER = ADC_IER_JEOSIE | ADC_IER_JQOVFIE;
    TIM1->DIER = TIM_DIER_UIE;
    NVIC_EnableIRQ(ADC1_2_IRQn);
    NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
    LL_ADC_INJ_StartConversion(ADC1);
    TIM1->CR1 |= TIM_CR1_CEN;
    return SYS_OK;
}
/** @brief 关闭输出、停止定时器和注入采样。 */
Sys_Status MotorSTM32_Stop(void *context)
{
    MotorSTM32 *self = context;
    PWM_Output_Disable(self);
    TIM1->CR1 &= ~TIM_CR1_CEN;
    TIM1->DIER = 0U;
    NVIC_DisableIRQ(ADC1_2_IRQn);
    NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
    ADC1->IER = 0U;
    if (ADC1->CR & ADC_CR_JADSTART) {
        LL_ADC_INJ_StopConversion(ADC1);
    }
    Sys_Status status = WaitClear(&ADC1->CR, ADC_CR_JADSTP | ADC_CR_JADSTART);
    self->sampling = false;
    return status;
}
/** @brief 用规则组读取一次母线 ADC。 */
Sys_Status MotorSTM32_ReadBus(void *context, uint16_t *raw)
{
    MotorSTM32 *self = context;
    if (!self->initialized) {
        return SYS_ERROR_NOT_READY;
    }
    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR;
    LL_ADC_REG_StartConversion(ADC1);
    uint32_t start = DWT_Cycles();
    while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {
        if (DWT_ElapsedUs(start) >= 1000U) {
            LL_ADC_REG_StopConversion(ADC1);
            (void)WaitClear(&ADC1->CR, ADC_CR_ADSTP | ADC_CR_ADSTART);
            return SYS_ERROR_TIMEOUT;
        }
    }
    *raw = (uint16_t)ADC1->DR;
    return SYS_OK;
}
/** @brief 在短临界区复制硬件状态与统计。 */
void MotorSTM32_Snapshot(void *context, MotorHardwareSnapshot *out)
{
    MotorSTM32 *self = context;
    const MotorSTM32Config *cfg = self->config;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    volatile uint32_t *ccr = &TIM1->CCR1;
    *out = (MotorHardwareSnapshot){.samples = self->samples,
                                   .errors = self->errors,
                                   .adc_errors = self->adc_errors,
                                   .deadline_errors = self->deadline_errors,
                                   .deadline_min_cycles = self->deadline_min == UINT32_MAX ? 0U : self->deadline_min,
                                   .interval_min_cycles = self->interval_min == UINT32_MAX ? 0U : self->interval_min,
                                   .interval_max_cycles = self->interval_max,
                                   .irq_max_cycles = self->irq_max,
                                   .timer_hz = cfg->timer_hz,
                                   .period_counts = TIM1->ARR,
                                   .compare_u = ccr[cfg->u_channel - 1U],
                                   .compare_v = ccr[cfg->v_channel - 1U],
                                   .compare_w = ccr[cfg->w_channel - 1U],
                                   .initialized = self->initialized,
                                   .sampling = self->sampling,
                                   .output_enabled = (TIM1->BDTR & TIM_BDTR_MOE) != 0U};
    __set_PRIMASK(mask);
}
/** @brief 读取成对样本、同步回调并检查更新截止。 */
void ADC1_2_IRQHandler(void)
{
    MotorSTM32 *self = irqMotor;
    uint32_t begin = DWT_Cycles();
    uint32_t master = ADC1->ISR, slave = ADC2->ISR;
    uint32_t remaining = TIM1->CNT;
    bool downcounting = (TIM1->CR1 & TIM_CR1_DIR) != 0U;
    if (!self || !self->sampling) {
        ADC1->ISR = master;
        ADC2->ISR = slave;
        return;
    }
    MotorSample sample = {.current_u_raw = (uint16_t)ADC2->JDR1,
                          .current_v_raw = (uint16_t)ADC1->JDR1,
                          .sequence = ++self->samples,
                          .completed_cycles = begin,
                          .flags = MOTOR_SAMPLE_VALID};
    if (!(master & ADC_ISR_JEOS) || !(slave & ADC_ISR_JEOS) || ((master | slave) & (ADC_ISR_JQOVF | ADC_ISR_OVR))) {
        sample.flags = MOTOR_SAMPLE_ADC_ERROR;
        ++self->adc_errors;
        ++self->errors;
        PWM_Output_Disable(self);
        TIM1->CR1 &= ~TIM_CR1_CEN;
        ADC1->IER = 0U;
    }
    ADC1->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC | ADC_ISR_JQOVF | ADC_ISR_OVR;
    ADC2->ISR = ADC_ISR_JEOS | ADC_ISR_JEOC | ADC_ISR_JQOVF | ADC_ISR_OVR;
    if (self->previous_cycles != 0U) {
        uint32_t interval = begin - self->previous_cycles;
        if (interval < self->interval_min) {
            self->interval_min = interval;
        }
        if (interval > self->interval_max) {
            self->interval_max = interval;
        }
    }
    self->previous_cycles = begin;
    uint32_t budget = downcounting && remaining > 128U ? remaining - 128U : 0U;
    if (budget < self->deadline_min) {
        self->deadline_min = budget;
    }
    self->callback(self->callback_context, &sample);
    uint32_t elapsed = DWT_Cycles() - begin;
    if (elapsed > self->irq_max) {
        self->irq_max = elapsed;
    }
    if (elapsed >= budget || (ADC1->ISR & ADC_ISR_JEOS)) {
        ++self->deadline_errors;
        ++self->errors;
        PWM_Output_Disable(self);
        TIM1->CR1 &= ~TIM_CR1_CEN;
        ADC1->IER = 0U;
    }
}
/** @brief 独立于 ADC 的 PWM 测试租期，ADC 回调异常也按更新周期关闭输出。 */
void TIM1_UP_TIM16_IRQHandler(void)
{
    TIM1->SR = ~TIM_SR_UIF;
    MotorSTM32 *self = irqMotor;
    if (self && self->lease_periods != 0U && --self->lease_periods == 0U) {
        PWM_Output_Disable(self);
    }
}

/** @brief 保存并屏蔽 IRQ，开始有界状态事务。 */
uint32_t MotorSTM32_CriticalEnter(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return mask;
}
/** @brief 结束事务并恢复原 IRQ 掩码。 */
void MotorSTM32_CriticalExit(uint32_t mask)
{
    __DMB();
    __set_PRIMASK(mask);
}
