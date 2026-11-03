/**
 * @file foc_transform.h
 * @brief 幅值不变 Clarke、Park 与逆 Park；单位 A/V/rad。
 *
 * @author kaouduo
 * @date 2026/10/31
 */
#ifndef FOC_TRANSFORM_H
#define FOC_TRANSFORM_H

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 执行幅值不变 Clarke 变换，W 按 -U-V 重建。
 * @param u U 相分量。
 * @param v V 相分量。
 * @param alpha α 轴输出。
 * @param beta β 轴输出。
 * @return 无。
 * @note 输入输出同单位，调用方保证有限值。
 */
void FOC_Clarke(float u, float v, float *alpha, float *beta);
/**
 * @brief 将 αβ 分量旋转到 dq 坐标系。
 * @param alpha α 轴分量。
 * @param beta β 轴分量。
 * @param sine 电角度正弦。
 * @param cosine 同一角度余弦。
 * @param d d 轴输出。
 * @param q q 轴输出。
 * @return 无。
 * @note 输入输出同单位，可复用同周期正余弦。
 */
void FOC_Park(float alpha, float beta, float sine, float cosine, float *d, float *q);
/**
 * @brief 将 dq 分量旋转到 αβ 坐标系。
 * @param d d 轴分量。
 * @param q q 轴分量。
 * @param sine 调制电角度正弦。
 * @param cosine 同一角度余弦。
 * @param alpha α 轴输出。
 * @param beta β 轴输出。
 * @return 无。
 * @note 输入输出同单位，使用实际调制角度。
 */
void FOC_InversePark(float d, float q, float sine, float cosine, float *alpha, float *beta);

#ifdef __cplusplus
}
#endif
#endif
