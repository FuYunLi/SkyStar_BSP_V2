/**
 * @file app_buzzer_demo.h
 * @brief 蜂鸣器曲目播放自检演示头文件
 * @note 对标 RocketPi 16_rocketpi_pwm_passive_buzzer。核心差异：原版在
 *       主循环中以 HAL_Delay 阻塞逐音符播放，本实现遵循全回调架构，
 *       以 MultiTimer 调度音符时值，播放期间系统其余任务不受阻塞。
 */

#ifndef __APP_BUZZER_DEMO_H
#define __APP_BUZZER_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蜂鸣器曲目播放演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_buzzer_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_BUZZER_DEMO_H */
