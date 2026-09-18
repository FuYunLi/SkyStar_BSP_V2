/**
 * @file app_sg90_demo.h
 * @brief SG90 舵机控制自检演示头文件
 * @note 对标 RocketPi 17_rocketpi_pwm_sg90。
 */

#ifndef __APP_SG90_DEMO_H
#define __APP_SG90_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化舵机控制演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_sg90_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SG90_DEMO_H */
