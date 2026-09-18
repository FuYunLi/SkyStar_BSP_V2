/**
 * @file app_mcu_temp_demo.h
 * @brief 片内温度采集自检演示头文件
 * @note 对标 RocketPi 20_rocketpi_adc_mcu_temperature，演示内部温度
 *       传感器通道的读取与统计平均。
 */

#ifndef __APP_MCU_TEMP_DEMO_H
#define __APP_MCU_TEMP_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化片内温度采集演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_mcu_temp_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MCU_TEMP_DEMO_H */
