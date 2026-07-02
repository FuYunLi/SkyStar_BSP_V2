/**
 * @file dev_potentiometer.h
 * @brief 旋转电位器设备驱动头文件
 * @note 基于 port_adc 采样并提供一阶低通滤波后的电位器物理开度百分比输出。
 */

#ifndef __DEV_POTENTIOMETER_H
#define __DEV_POTENTIOMETER_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化旋转电位器设备驱动
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_ERROR 硬件初始化失败
 */
bsp_status_t dev_potentiometer_init(void);

/**
 * @brief 获取滤波后的电位器电压值（单位：mV）
 * @param[out] voltage_mv 存储毫伏电压指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 外设未初始化或读取错误
 */
bsp_status_t dev_potentiometer_get_voltage(uint32_t *voltage_mv);

/**
 * @brief 获取滤波后的电位器旋转百分比（0.0% ~ 100.0%）
 * @param[out] percent 存储开度百分比的浮点数指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 外设未初始化或读取错误
 */
bsp_status_t dev_potentiometer_get_percent(float *percent);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_POTENTIOMETER_H */
