/**
 * @file app_standby_demo.h
 * @brief 待机低功耗自检演示头文件
 * @note 对标 RocketPi 39_rocketpi_standby_wkup，演示 STANDBY 模式进入、
 *       WKUP 唤醒源配置与复位来源判定。
 */

#ifndef __APP_STANDBY_DEMO_H
#define __APP_STANDBY_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化待机低功耗演示模块
 * @note 读取 PWR 待机标志（SBF）判定本次复位来源并打印
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_standby_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_STANDBY_DEMO_H */
