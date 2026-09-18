/**
 * @file dev_stepper.h
 * @brief TMC2209 步进电机驱动头文件
 * @note 对标官方出厂 pwm_step_motor_thread。控制模型：STEP 脉冲
 *       （TIM2_CH1/PA15，频率即步进速率）+ DIR 方向（PD4）+
 *       ENN 使能（PD7，低有效）。细分与工作电流由 SW3 拨码配置
 *       （默认 8 细分/静音模式），微步热敏串口调参暂未启用。
 *       编码器闭环（PE9/PE11 TIM1 + Z 相 PE10）为扩展项。
 *       【未验证】步进电机尚未接线实测。
 */

#ifndef __DEV_STEPPER_H
#define __DEV_STEPPER_H

#include "bsp_board.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化步进电机（禁用状态，正转方向）
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_stepper_init(void);

/**
 * @brief 使能或禁用步进驱动
 * @param enable true = 使能（ENN 拉低）
 */
bsp_status_t dev_stepper_enable(bool enable);

/**
 * @brief 设置旋转方向
 * @param cw true = 顺时针（DIR 高）
 */
bsp_status_t dev_stepper_set_dir(bool cw);

/**
 * @brief 按指定步进速率移动指定步数（阻塞式）
 * @note 通过 PWM 连续脉冲 + 定时停止实现，步数为按时间折算的
 *       近似值；阻塞于调用上下文，仅限 Shell 等主上下文使用。
 *       64 细分抖动与机械惯量允许的频率范围约 100-8000Hz。
 * @param steps 步数（含细分）
 * @param freq_hz 脉冲频率（Hz）
 * @retval BSP_OK 完成
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_stepper_move_steps(uint32_t steps, uint32_t freq_hz);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_STEPPER_H */
