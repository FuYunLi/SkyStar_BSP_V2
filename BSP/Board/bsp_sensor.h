/**
 * @file bsp_sensor.h
 * @brief 通用温湿度传感器板级支持抽象接口
 * @note 隔离底层具体物理芯片，应用层只能通过此接口访问环境传感器
 */

#ifndef __BSP_SENSOR_H
#define __BSP_SENSOR_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化温湿度环境传感器
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_sensor_init(void);

/**
 * @brief  读取当前的温湿度环境数据
 * @param[out] temperature 存放温度结果的指针（单位：摄氏度）
 * @param[out] humidity 存放湿度结果的指针（单位：%RH）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_sensor_read_environmental(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SENSOR_H */
