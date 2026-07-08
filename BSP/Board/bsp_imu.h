/**
 * @file bsp_imu.h
 * @brief 板级姿态传感器 (ICM-42688-P) 服务抽象接口
 * @note 封装硬件选通、轮询更新及互补滤波计算，提供上层获取姿态与原始数据的防竞态接口。
 */

#ifndef __BSP_IMU_H
#define __BSP_IMU_H

#include "bsp_board.h"
#include <stdint.h>

/* ================================================================
 * 数据类型定义
 * ================================================================ */

/**
 * @brief 物理原始读数结构体
 */
typedef struct
{
    float accel_x;  /* X 轴加速度，单位 g */
    float accel_y;  /* Y 轴加速度，单位 g */
    float accel_z;  /* Z 轴加速度，单位 g */
    float gyro_x;   /* X 轴角速度，单位 dps */
    float gyro_y;   /* Y 轴角速度，单位 dps */
    float gyro_z;   /* Z 轴角速度，单位 dps */
    float temp;     /* 芯片内部温度，单位 ℃ */
} bsp_imu_raw_t;

/**
 * @brief 姿态角结构体
 */
typedef struct
{
    float pitch;    /* 俯仰角 Pitch，单位 度 */
    float roll;     /* 横滚角 Roll，单位 度 */
} bsp_imu_attitude_t;

/* ================================================================
 * API 声明
 * ================================================================ */

/**
 * @brief 初始化 IMU 板级支持服务
 * @note 会在内部执行 PCA9555 总线切换并接通 SPI2 通道，再对物理驱动进行初始化。
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
bsp_status_t bsp_imu_init(void);

/**
 * @brief 姿态解算周期更新任务
 * @note 恒定调用周期为 10ms (100Hz)，包含互补滤波计算。
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
bsp_status_t bsp_imu_update(void);

/**
 * @brief 获取最新采样并换算后的六轴物理原始读数
 * @note 该接口内置临界区中断并发安全保护。
 * @param raw 存储原始读数的结构体指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数为空指针
 *         - BSP_ERROR 服务未初始化
 */
bsp_status_t bsp_imu_get_raw(bsp_imu_raw_t *raw);

/**
 * @brief 获取最新的俯仰角和横滚角姿态信息
 * @note 该接口内置临界区中断并发安全保护。
 * @param att 存储姿态角的结构体指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数为空指针
 *         - BSP_ERROR 服务未初始化
 */
/**
 * @brief 挂起 IMU 服务周期采样与总线访问
 */
void bsp_imu_suspend(void);

/**
 * @brief 恢复 IMU 服务周期采样与总线访问
 */
void bsp_imu_resume(void);

#endif /* __BSP_IMU_H */

