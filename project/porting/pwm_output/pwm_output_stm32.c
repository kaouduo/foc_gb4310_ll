/**
 * @file pwm_output_stm32.c
 * @brief 占空比预装载、限时测试与紧急关断。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#include "pwm_output_stm32.h"
#include <math.h>
#include <stddef.h>
#include "motor_sampling_stm32.h"
#define PHASE_CHANNELS                                                                                                 \
    (TIM_CCER_CC1E | TIM_CCER_CC1NE | TIM_CCER_CC2E | TIM_CCER_CC2NE | TIM_CCER_CC3E | TIM_CCER_CC3NE)
_Static_assert(TIM1_BASE == 0x40012C00UL && offsetof(TIM_TypeDef, CCER) == 0x20U &&
                       offsetof(TIM_TypeDef, BDTR) == 0x44U,
               "Fault assembly TIM1 layout");
/** @brief 直接关断 TIM1 功率输出，不依赖上下文。 */
void PWM_Output_EmergencyOff(void)
{
    TIM1->BDTR &= ~TIM_BDTR_MOE;
    TIM1->CCER &= ~PHASE_CHANNELS;
    __DSB();
}
/** @brief 清除 MOE、相输出 CCER 和剩余周期。 */
void PWM_Output_Disable(void *context)
{
    MotorSTM32 *self = context;
    PWM_Output_EmergencyOff();
    self->lease_periods = 0U;
}
/** @brief 原子更新三相占空比预装载值。 */
Sys_Status PWM_Output_Write(void *context, float u, float v, float w)
{
    MotorSTM32 *self = context;
    if (!self->initialized) {
        return SYS_ERROR_NOT_READY;
    }
    const MotorSTM32Config *cfg = self->config;
    if (!isfinite(u) || !isfinite(v) || !isfinite(w) || u < cfg->duty_min - 1e-6f || v < cfg->duty_min - 1e-6f ||
        w < cfg->duty_min - 1e-6f || u > cfg->duty_max + 1e-6f || v > cfg->duty_max + 1e-6f ||
        w > cfg->duty_max + 1e-6f) {
        PWM_Output_Disable(self);
        return SYS_ERROR_PARAM;
    }
    if (__get_IPSR() && self->sampling && (!(TIM1->CR1 & TIM_CR1_DIR) || TIM1->CNT <= 128U)) {
        PWM_Output_Disable(self);
        return SYS_ERROR_TIMEOUT;
    }
    /* 只修正浮点舍入（远小于一个 CCR count），不裁剪超出调制范围的真实请求。 */
    u = u < cfg->duty_min ? cfg->duty_min : (u > cfg->duty_max ? cfg->duty_max : u);
    v = v < cfg->duty_min ? cfg->duty_min : (v > cfg->duty_max ? cfg->duty_max : v);
    w = w < cfg->duty_min ? cfg->duty_min : (w > cfg->duty_max ? cfg->duty_max : w);
    uint32_t counts[3] = {(uint32_t)(u * TIM1->ARR + 0.5f), (uint32_t)(v * TIM1->ARR + 0.5f),
                          (uint32_t)(w * TIM1->ARR + 0.5f)};
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    uint32_t cr1 = TIM1->CR1;
    TIM1->CR1 = cr1 | TIM_CR1_UDIS;
    volatile uint32_t *ccr = &TIM1->CCR1;
    ccr[cfg->u_channel - 1U] = counts[0];
    ccr[cfg->v_channel - 1U] = counts[1];
    ccr[cfg->w_channel - 1U] = counts[2];
    TIM1->CR1 = cr1;
    __set_PRIMASK(mask);
    return SYS_OK;
}
/** @brief 按有限 PWM 周期数使能三相输出。 */
Sys_Status PWM_Output_Enable(void *context, uint32_t periods)
{
    MotorSTM32 *self = context;
    if (!self->initialized || !self->sampling) {
        return SYS_ERROR_NOT_READY;
    }
    if (periods == 0U || periods > 90U * self->config->pwm_hz) {
        return SYS_ERROR_PARAM;
    }
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (self->errors != 0U || (TIM1->BDTR & TIM_BDTR_MOE)) {
        __set_PRIMASK(mask);
        return SYS_ERROR_BUSY;
    }
    self->lease_periods = periods;
    TIM1->CCER |= PHASE_CHANNELS;
    TIM1->BDTR |= TIM_BDTR_MOE;
    __set_PRIMASK(mask);
    return SYS_OK;
}

/** @brief 按最多一秒的租期使能诊断输出。 */
Sys_Status PWM_Output_EnableTest(void *context, uint32_t periods)
{
    MotorSTM32 *self = context;
    if (periods > self->config->pwm_hz) {
        return SYS_ERROR_PARAM;
    }
    return PWM_Output_Enable(context, periods);
}
