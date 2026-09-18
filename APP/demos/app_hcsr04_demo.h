/**
 * @file app_hcsr04_demo.h
 * @brief HC-SR04 超声波测距自检演示头文件
 * @note 对标 RocketPi 19_rocketpi_hcsr04。【未验证】外部模块尚未接线
 *       实测，示例接线 PD11(TRIG)/PA8(ECHO)。
 */

#ifndef __APP_HCSR04_DEMO_H
#define __APP_HCSR04_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化超声波测距演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_hcsr04_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_HCSR04_DEMO_H */
