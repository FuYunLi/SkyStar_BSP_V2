/**
 * @file dev_icm42688.h
 * @brief ICM-42688-P 六轴姿态传感器底层设备驱动头文件
 * @note 提供寄存器定义、物理读数结构体及初始化、采样 API 声明，隔离硬件细节。
 */

#ifndef __DEV_ICM42688_H
#define __DEV_ICM42688_H

#include "bsp_board.h"
#include <stdint.h>

/* ================================================================
 * 数据类型定义
 * ================================================================ */

/**
 * @brief ICM-42688-P 物理读数结构体
 */
typedef struct
{
    float accel_x_g;   /* X 轴加速度 (g) */
    float accel_y_g;   /* Y 轴加速度 (g) */
    float accel_z_g;   /* Z 轴加速度 (g) */
    float gyro_x_dps;  /* X 轴角速度 (dps) */
    float gyro_y_dps;  /* Y 轴角速度 (dps) */
    float gyro_z_dps;  /* Z 轴角速度 (dps) */
    float temp_c;      /* 芯片温度 (℃) */
} icm42688_data_t;

/* ================================================================
 * API 声明
 * ================================================================ */

/**
 * @brief 初始化姿态传感器
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_ERROR 失败或设备不存在
 */
bsp_status_t icm42688_init(void);

/**
 * @brief 读取最新的 6 轴加速度、角速度与芯片温度数据
 * @param data 指向存储读取结果的数据结构体指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数为空指针
 *         - BSP_ERROR 读取错误
 */
bsp_status_t icm42688_read_data(icm42688_data_t *data);

#endif /* __DEV_ICM42688_H */

