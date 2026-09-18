/**
 * @file app_motor_demo.h
 * @brief 直流电机控制自检演示头文件
 * @note 对标 RocketPi 18_rocketpi_pwm_motor。【未验证】外部电机尚未
 *       接线实测。
 */

#ifndef __APP_MOTOR_DEMO_H
#define __APP_MOTOR_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化直流电机控制演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_motor_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MOTOR_DEMO_H */
