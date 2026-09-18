/**
 * @file app_ds18b20_demo.h
 * @brief DS18B20 温度采集自检演示头文件
 * @note 【未验证】外部传感器尚未接线实测。
 */

#ifndef __APP_DS18B20_DEMO_H
#define __APP_DS18B20_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 DS18B20 演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_ds18b20_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DS18B20_DEMO_H */
