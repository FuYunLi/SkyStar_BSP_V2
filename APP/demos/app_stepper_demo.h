/**
 * @file app_stepper_demo.h
 * @brief TMC2209 步进电机自检演示头文件
 * @note 对标官方出厂 STEPPER 例程。【未验证】步进电机尚未接线实测。
 */

#ifndef __APP_STEPPER_DEMO_H
#define __APP_STEPPER_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化步进电机演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_stepper_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_STEPPER_DEMO_H */
