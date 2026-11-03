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
#include "app_motor_calibration.h"
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
typedef enum {
    APP_MOTOR_IDLE,      /**< 空闲或已正常停机。 */
    APP_MOTOR_QUEUED,    /**< 命令已接受，等待监督任务准备。 */
    APP_MOTOR_PREPARING, /**< 关闭输出采集零偏并验证启动条件。 */
    APP_MOTOR_OPEN,      /**< 开环执行器运行中，也用于校准电压输出。 */
    APP_MOTOR_STOPPING,  /**< 输出已关闭，等待模式退出和采样收尾。 */
    APP_MOTOR_FAULT      /**< 故障锁存，禁止自动重新启动。 */
} App_MotorRunState;
typedef enum {
    APP_MOTOR_REASON_NONE,
    APP_MOTOR_REASON_DONE,
    APP_MOTOR_REASON_STOP,
    APP_MOTOR_REASON_ZERO,
    APP_MOTOR_REASON_SAMPLE,
    APP_MOTOR_REASON_RAIL,
    APP_MOTOR_REASON_CURRENT,
    APP_MOTOR_REASON_UNDER_VOLTAGE,
    APP_MOTOR_REASON_OVER_VOLTAGE,
    APP_MOTOR_REASON_STALE,
    APP_MOTOR_REASON_COMPUTE,
    APP_MOTOR_REASON_DEADLINE,
    APP_MOTOR_REASON_CALIBRATION
} App_MotorReason;
typedef struct {
    float frequency_hz;   /**< 有符号电频率，单位 Hz，零表示固定磁场。 */
    float voltage_v;      /**< 非负电压幅值请求，单位 V，按实时母线限幅。 */
    uint32_t duration_ms; /**< 运行总时长，范围 100～10000 ms。 */
    uint32_t ramp_ms;     /**< 上升/下降各自的斜坡时间，单位 ms，不超过总时长一半。 */
} App_MotorOpenCommand;
typedef struct {
    App_MotorRunState state;
    App_MotorReason reason;
    Sys_Status status;
    uint32_t faults;        /**< 本模块锁存的 System_Fault 位。 */
    uint32_t frames;        /**< 已运行快速控制帧数。 */
    uint32_t target_frames; /**< 本次最大控制帧数，不包含零偏准备。 */
    uint32_t fault_frame;   /**< 首故障对应控制帧号。 */
    uint32_t encoder_errors, bus_updates;
    float bus_v;                     /**< 最近母线电压，单位 V。 */
    float voltage_limit_v;           /**< 母线及调制范围对应幅值上限，单位 V。 */
    float peak_current_a;            /**< 本次三相估算电流绝对值峰值，单位 A。 */
    float applied_voltage_v;         /**< 最近开环 d 轴电压，单位 V。 */
    float current_u_a;               /**< 最近 U 相反馈，单位 A。 */
    float current_v_a;               /**< 最近 V 相反馈，单位 A。 */
    float current_w_a;               /**< 最近重建 W 相反馈，单位 A，等于 -U-V。 */
    int32_t mechanical_delta_counts; /**< 相对运行起点的展开编码器计数，32768 counts/圈。 */
    uint16_t bus_raw;                /**< 最近母线 ADC 原始计数。 */
    uint16_t encoder_raw;            /**< 最近有效 15 位编码器计数。 */
    uint16_t fault_raw_u;            /**< 首故障 U 相 ADC 计数。 */
    uint16_t fault_raw_v;            /**< 首故障 V 相 ADC 计数。 */
    bool encoder_valid;              /**< 最近一次编码器读取是否成功。 */
} App_MotorRunSnapshot;
/**
 * @brief 排队提交 2 Hz、5 V 编码器校准。
 * @return SYS_OK 仅表示接受；忙或故障时拒绝。
 * @note 计算通过后由监督任务先停机再保存，取消和质量失败不写新记录。
 */
Sys_Status App_Motor_RequestCalibration(void);
/**
 * @brief 复制已持有参数和本轮校准进度。
 * @param calibration 上次有效参数输出。
 * @param progress 本轮候选结果及失败原因输出。
 * @return 无。
 * @note 任务调用，在短临界区内复制一致状态。
 */
void App_Motor_GetCalibration(MotorCalibration *calibration, MotorCalSnapshot *progress);
/**
 * @brief 排队提交有限时间开环请求。
 * @param command 电频率 Hz、幅值 V、持续时间和斜坡 ms。
 * @return SYS_OK 仅表示接受；非法参数、忙或故障时拒绝。
 * @note 任务调用，最大持续 10000 ms，实际启动由监督任务执行。
 */
Sys_Status App_Motor_RequestOpen(const App_MotorOpenCommand *command);
/**
 * @brief 取消启动请求并同步收尾已运行的模式。
 * @return 无。
 * @note 任务调用，立即关闭功率输出；准备期采样由采集所有者响应取消后收尾。
 */
void App_Motor_RequestStop(void);
/**
 * @brief 在停止且母线有效时显式清除本模块故障。
 * @return SYS_OK、BUSY、NOT_READY 或母线读取错误。
 * @note 任务调用，保持停止，不自动恢复运行。
 */
Sys_Status App_Motor_ClearFault(void);
/**
 * @brief 复制运行或上次停机的遥测快照。
 * @param snapshot 一致快照输出。
 * @return 无。
 * @note 任务调用，空闲时物理量可能是上次冻结值。
 */
void App_Motor_GetRunSnapshot(App_MotorRunSnapshot *snapshot);
/**
 * @brief 推进准备、保护、校准与停机保存。
 * @return 无。
 * @note 仅唯一 1 ms 电机监督任务调用，Shell 不得并发执行。
 */
void App_Motor_Process(void);
/**
 * @brief 读取编码器样本，准备或运行时只读缓存。
 * @param sample 成功时写入编码器样本。
 * @return SYS_OK 或缓存/器件读取错误。
 * @note 任务调用；空闲时直接访问编码器 SPI。
 */
Sys_Status App_Motor_ReadEncoder(MT6826S_Sample *sample);

#ifdef __cplusplus
}
#endif
#endif
