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

/**
 * @brief 主动读取器件 ID（WHO_AM_I），不附带任何校验与配置改动
 * @param id 存入读到的原始 ID 字节（ICM-42688-P 正常应为 0x47）
 * @return bsp_status_t 执行结果
 * @note  专给自检/定位用：init 失败时必须能分清“SPI 事务根本没成”与“读到了错的 ID”，
 *        二者对应完全不同的排查方向（总线/供电/片选 vs 型号/寄存器 Bank/复位状态）。
 *        本函数不会改变器件任何寄存器状态，可安全重复调用
 */
bsp_status_t icm42688_read_chip_id(uint8_t *id);

#endif /* __DEV_ICM42688_H */

