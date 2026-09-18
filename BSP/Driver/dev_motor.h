/**
 * @file dev_motor.h
 * @brief AT8236 直流电机驱动头文件
 * @note 对标 RocketPi 18_rocketpi_pwm_motor（原版为 L9110，本板为
 *       AT8236 双路驱动）。电机1 使用 TIM9 双通道（PE5/PE6），
 *       IN1/IN2 PWM-PWM 控制方向与速度；编码器 PB4/PC7 挂 TIM3，
 *       闭环控制暂未启用。
 *       【未验证】外部电机负载尚未上板实测，正反转逻辑以 AT8236
 *       数据手册真值表为准。
 */

#ifndef __DEV_MOTOR_H
#define __DEV_MOTOR_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 类型定义
 * ================================================================ */

typedef enum
{
    DEV_MOTOR_1 = 0, /* TIM9_CH1/CH2 (PE5/PE6) */
    DEV_MOTOR_MAX
} dev_motor_id_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化指定电机通道，初始停止状态
 * @param id 电机逻辑 ID
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_motor_init(dev_motor_id_t id);

/**
 * @brief 设置电机转速与方向
 * @param id 电机逻辑 ID
 * @param speed_permille 速度千分比 -1000 ~ 1000：
 *       正值正转（IN1=PWM, IN2=0），负值反转，0 停止
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_motor_set_speed(dev_motor_id_t id, int16_t speed_permille);

/**
 * @brief 停止电机（滑行：IN1=IN2=0）
 */
bsp_status_t dev_motor_stop(dev_motor_id_t id);

/**
 * @brief 刹车（IN1=IN2=满占空比）
 */
bsp_status_t dev_motor_brake(dev_motor_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_MOTOR_H */
