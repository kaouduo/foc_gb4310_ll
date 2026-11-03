/**
 * @file app_motor.h
 * @brief 硬件诊断、有界开环电压运行与遥测。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef APP_MOTOR_H
#define APP_MOTOR_H

/* 构建系统显式选择；非工程使用者默认仅编译生产能力。 */
#ifndef FOC_ENABLE_MOTOR_DIAGNOSTICS
#define FOC_ENABLE_MOTOR_DIAGNOSTICS 0
#endif
#include "motor_io.h"
#include "mt6826s_drv.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    APP_MOTOR_TEST_CURRENT,
    APP_MOTOR_TEST_ZERO,
    APP_MOTOR_TEST_SYNC,
    APP_MOTOR_TEST_CHAIN,
    APP_MOTOR_TEST_PWM_U,
    APP_MOTOR_TEST_PWM_V,
    APP_MOTOR_TEST_PWM_W,
    APP_MOTOR_TEST_PWM_ALL
} App_MotorTest;
typedef struct {
    MotorHardwareSnapshot hardware; /**< 停止诊断后的硬件与统计副本。 */
    uint32_t collected;             /**< 已收集的成对 ADC 样本数。 */
    uint32_t callback_errors;       /**< 样本标志、序号或计算错误数。 */
    uint32_t encoder_errors;        /**< 全链路诊断中的编码器读取错误数。 */
    uint32_t elapsed_ms;            /**< 本次诊断采样等待时长，单位 ms。 */
    uint32_t encoder_max_cycles;    /**< 单次编码器读取最长 CPU 周期数。 */
    uint32_t control_max_cycles;    /**< 单次诊断计算最长 CPU 周期数。 */
    uint32_t bus_reads;             /**< 同步诊断期间额外规则组读取次数。 */
    uint16_t min_u;                 /**< U 相 ADC 原始最小计数。 */
    uint16_t max_u;                 /**< U 相 ADC 原始最大计数。 */
    uint16_t min_v;                 /**< V 相 ADC 原始最小计数。 */
    uint16_t max_v;                 /**< V 相 ADC 原始最大计数。 */
    float mean_u;                   /**< U 相 ADC 平均计数，未扣零偏。 */
    float mean_v;                   /**< V 相 ADC 平均计数，未扣零偏。 */
    float offset_u;                 /**< 当前 U 相零偏，单位 ADC count。 */
    float offset_v;                 /**< 当前 V 相零偏，单位 ADC count。 */
    float current_u_a;              /**< 采样平均/PWM末帧 U 相电流，单位 A；零偏无效时为 0。 */
    float current_v_a;              /**< 采样平均/PWM末帧 V 相电流，单位 A；零偏无效时为 0。 */
    float bus_v;                    /**< 停机采样开始/PWM末次母线电压，单位 V。 */
    bool zero_valid;                /**< 电流零偏是否已通过检查。 */
} App_MotorTestResult;
/**
 * @brief 构造模式、注册采样回调并加载校准参数。
 * @return SYS_OK 或回调绑定错误。
 * @note 启动任务调用一次；BSP 电机和 Flash 已初始化，输出保持关闭。
 */
Sys_Status App_Motor_Init(void);
/**
 * @brief 独占执行电流、同步链路或限时 PWM 诊断。
 * @param test 诊断种类。
 * @param result 成功进入诊断后写入采样和硬件快照。
 * @return SYS_OK、BUSY、NOT_READY 或诊断错误。
 * @note Shell 串行任务调用，退出时关闭输出；Debug PWM 诊断复用运行执行器及其全部保护。
 */
Sys_Status App_Motor_Test(App_MotorTest test, App_MotorTestResult *result);
/**
 * @brief 读取母线诊断值，运行期间只读监督任务缓存。
 * @param raw ADC 原始计数输出。
 * @param volts 换算电压输出，单位 V。
 * @return SYS_OK、NOT_READY 或硬件读取错误。
 * @note 任务调用，不与运行中的规则组 ADC 读取竞争。
 */
Sys_Status App_Motor_ReadBus(uint16_t *raw, float *volts);
/**
 * @brief 执行纯软件算法和模式绑定自检。
 * @param checks 已通过的软件检查计数。
 * @return SYS_OK 或首个检查错误。
 * @note 不操作硬件；调用不代表电机实际输出已经关闭。
 */
#if FOC_ENABLE_MOTOR_DIAGNOSTICS
Sys_Status App_Motor_TestAlgorithms(uint32_t *checks);
#endif
/**
 * @brief 立即关闭输出并取消启动；采样由其所有者完成收尾。
 * @return 无。
 * @note 任务调用；不自动清除故障或重新运行。
 */
void App_Motor_Stop(void);
/** @brief 停机状态下读取一次编码器；只有 Shell 任务调用。 */
Sys_Status App_Motor_ReadEncoder(MT6826S_Sample *sample);
#ifdef __cplusplus
}
#endif
#endif
