/**
 * @file dev_sg90.h
 * @brief SG90 舵机驱动头文件
 * @note 对标 RocketPi 17_rocketpi_pwm_sg90。双路舵机挂载 TIM12
 *       （CH1/PB14、CH2/PB15），50Hz PWM，脉宽 500-2500µs 对应 0-180°。
 */

#ifndef __DEV_SG90_H
#define __DEV_SG90_H

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
    DEV_SG90_1 = 0, /* TIM12_CH1 / PB14 */
    DEV_SG90_2,     /* TIM12_CH2 / PB15 */
    DEV_SG90_MAX
} dev_sg90_id_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化指定舵机通道并归中 90°
 * @param id 舵机逻辑 ID
 * @retval BSP_OK 初始化成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_sg90_init(dev_sg90_id_t id);

/**
 * @brief 设置舵机角度
 * @param id 舵机逻辑 ID
 * @param angle 目标角度（0-180），越界自动钳位
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_sg90_set_angle(dev_sg90_id_t id, uint8_t angle);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_SG90_H */
