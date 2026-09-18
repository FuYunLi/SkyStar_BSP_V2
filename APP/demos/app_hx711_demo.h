/**
 * @file app_hx711_demo.h
 * @brief HX711 称重采集自检演示头文件
 * @note 对标官方出厂 hx711_driver.c。【未验证】外部称重传感器尚未
 *       接线实测。
 */

#ifndef __APP_HX711_DEMO_H
#define __APP_HX711_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 HX711 称重演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_hx711_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_HX711_DEMO_H */
