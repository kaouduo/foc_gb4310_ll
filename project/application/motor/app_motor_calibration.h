/**
 * @file app_motor_calibration.h
 * @brief 编码器校准流程与质量评估，任务调用、不访问硬件。
 *
 * @author kaouduo
 * @date 2026/11/01
 */
#ifndef APP_MOTOR_CALIBRATION_H
#define APP_MOTOR_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif
#define MOTOR_CAL_FREQUENCY_HZ 2            /**< 有符号电频率常量，反向阶段使用其负值，单位 Hz。 */
#define MOTOR_CAL_VOLTAGE_V 5.0f            /**< 初始校准幅值，单位 V，执行器仍按母线动态限幅。 */
#define MOTOR_CAL_MAX_POLES 64U             /**< 本轮自动辨识支持的极对数上界。 */
#define MOTOR_CAL_POLE_TOLERANCE 0.20f      /**< 拟合极对数与最近整数的最大偏差。 */
#define MOTOR_CAL_DIRECTION_TOLERANCE 0.15f /**< 正反向拟合极对数的最大差值。 */
#define MOTOR_CAL_FIT_RMS_MAX_RAD 0.35f     /**< 轨迹拟合残差 RMS 门槛，电角度 rad。 */
/* 两方向各 p/fe，25% 行程裕量，加 4 s 对齐/通道检查/收尾。 */
#define MOTOR_CAL_MAX_MS (2U * MOTOR_CAL_MAX_POLES * 1000U / MOTOR_CAL_FREQUENCY_HZ * 5U / 4U + 4000U)
typedef enum {
    MOTOR_CAL_IDLE,            /**< 本次尚未开始；与存储参数是否有效无关。 */
    MOTOR_CAL_ALIGN,           /**< 固定磁场对齐和第一组电流检查。 */
    MOTOR_CAL_MAP_MOVE,        /**< 移动约 120° 电角度。 */
    MOTOR_CAL_MAP_HOLD,        /**< 保持并检查第二组电流方向。 */
    MOTOR_CAL_RETURN,          /**< 回到初始电角度。 */
    MOTOR_CAL_ZERO_HOLD,       /**< 记录初始静态零位。 */
    MOTOR_CAL_FORWARD,         /**< 正向扫描一机械圈。 */
    MOTOR_CAL_FORWARD_REALIGN, /**< 正向扫描后回到初始电角度。 */
    MOTOR_CAL_FORWARD_HOLD,    /**< 记录正向静态停靠零位。 */
    MOTOR_CAL_REVERSE,         /**< 反向扫描一机械圈。 */
    MOTOR_CAL_REVERSE_REALIGN, /**< 反向扫描后回到初始电角度。 */
    MOTOR_CAL_REVERSE_HOLD,    /**< 记录反向零位并检查质量。 */
    MOTOR_CAL_RAMP_DOWN,       /**< 电压按 200 ms 下降。 */
    MOTOR_CAL_SAVING,          /**< 计算通过，等待调用方停机保存。 */
    MOTOR_CAL_DONE,            /**< 保存及读回核对通过。 */
    MOTOR_CAL_FAILED,          /**< 计算或保存失败，候选不可用。 */
    MOTOR_CAL_CANCELLED        /**< 显式取消，不覆盖旧参数。 */
} MotorCalPhase;
typedef enum {
    MOTOR_CAL_ERROR_NONE,
    MOTOR_CAL_ERROR_STOP,
    MOTOR_CAL_ERROR_FEEDBACK,
    MOTOR_CAL_ERROR_STALL,
    MOTOR_CAL_ERROR_TIMEOUT,
    MOTOR_CAL_ERROR_CURRENT_MAP,
    MOTOR_CAL_ERROR_QUALITY,
    MOTOR_CAL_ERROR_STORAGE,
    MOTOR_CAL_ERROR_RUNTIME
} MotorCalError;
typedef struct {
    uint32_t pole_pairs;   /**< 辨识后的整数极对数，范围 1～64。 */
    int32_t direction;     /**< 编码器机械角到电角度的符号，取 +1 或 -1。 */
    float offset_rad;      /**< 全圈双向平均电角度零偏，范围 [0, 2π)，单位 rad。 */
    float forward_poles;   /**< 正向轨迹拟合的极对数绝对值，取整前结果。 */
    float reverse_poles;   /**< 反向轨迹拟合的极对数绝对值，取整前结果。 */
    float forward_rms_rad; /**< 正向拟合直线残差 RMS，电角度 rad。 */
    float reverse_rms_rad; /**< 反向拟合直线残差 RMS，电角度 rad。 */
    bool valid;            /**< 本结果是否通过质量检查。 */
    bool saved;            /**< 是否已保存并读回确认；不代表双页掉电回滚能力。 */
} MotorCalibration;
typedef struct {
    MotorCalPhase phase;        /**< 当前阶段；启动加载有效记录时仍可为 IDLE。 */
    MotorCalPhase failed_phase; /**< 首次中止所处阶段。 */
    MotorCalError error;
    Sys_Status status;
    uint32_t elapsed_ms;         /**< 按控制帧换算的校准时间，单位 ms。 */
    uint32_t stable_span_counts; /**< 最近保持窗口机械计数峰峰值。 */
    float zero_spread_rad;       /**< 三个静态零位的最大两两圆周差，电角度 rad。 */
    float return_error_rad;      /**< 双向停靠中心相对初始零位误差，电角度 rad。 */
    float directional_gap_rad;   /**< 正反全圈偏置均值差，电角度 rad。 */
    float static_error_rad;      /**< 静态零位与全圈偏置的最大差，电角度 rad。 */
    int32_t forward_counts;      /**< 正向一圈的有符号机械计数。 */
    int32_t reverse_counts;      /**< 反向一圈的有符号机械计数。 */
    MotorCalibration result;     /**< 本轮候选结果；须检查 valid 和 saved。 */
} MotorCalSnapshot;
typedef struct {
    uint32_t frames;            /**< 本轮控制帧数，不包含准备零偏阶段。 */
    int32_t mechanical_counts;  /**< 含初始绝对编码器值的展开计数，32768 counts/机械圈。 */
    float electrical_angle_rad; /**< 当前实际输出电角度，单位 rad。 */
    float current_u_a;          /**< 最近有效 U 相电流，单位 A。 */
    float current_v_a;          /**< 最近有效 V 相电流，单位 A。 */
    bool encoder_valid;         /**< 编码器通信、状态和本次跳变检查均通过。 */
} MotorCalInput;
typedef struct {
    bool change_command; /**< 调用方应将下述命令交接给下一快速控制周期。 */
    bool finished;       /**< 校准计算完成，调用方应先停机再保存；不是已保存标志。 */
    float frequency_hz;  /**< 下一阶段电频率，单位 Hz，可为负或 0。 */
    float voltage_v;     /**< 下一阶段 d 轴目标幅值，单位 V。 */
} MotorCalAction;
/**
 * @brief 清空本轮统计并进入初始对齐阶段。
 * @param pwm_hz 固定控制频率，单位 Hz，本工程为 20000。
 * @return 无。
 * @note 模块为单实例，任务唯一拥有；频率必须固定且为 1000 的整数倍。
 */
void MotorCal_Begin(uint32_t pwm_hz);
/**
 * @brief 使用一个监督观测推进校准阶段并生成动作。
 * @param input 展开机械角和当前电角度等一致观测。
 * @param action 下一快速周期命令或停机保存请求。
 * @return SYS_OK 或反馈、超时、质量错误。
 * @note 任务调用，不访问硬件；调用方执行关断和命令交接。
 */
Sys_Status MotorCal_Step(const MotorCalInput *input, MotorCalAction *action);
/**
 * @brief 将本轮标记为失败或取消，候选参数失效。
 * @param error 停止原因。
 * @param status 对应统一结果码。
 * @return 无。
 * @note 不关闭硬件，不改变上次已保存参数。
 */
void MotorCal_Abort(MotorCalError error, Sys_Status status);
/**
 * @brief 复制本轮校准进度与候选结果。
 * @param snapshot 快照输出。
 * @return 无。
 * @note 跨任务读取的一致性由调用方保护。
 */
void MotorCal_GetSnapshot(MotorCalSnapshot *snapshot);
/**
 * @brief 发布保存及读回结果。
 * @param status Flash 保存与读回结果。
 * @return 无。
 * @note 调用方已停止输出和采样，仅在计算通过后的保存阶段调用。
 */
void MotorCal_SetSaved(Sys_Status status);
#ifdef __cplusplus
}
#endif
#endif
