/**
 * @file app_imu_demo.h
 * @brief 自检演示模块——板载姿态传感器 Shell 自检指令模块头文件
 * @note 提供自检 Demo 初始化入口声明。
 */

#ifndef __APP_IMU_DEMO_H
#define __APP_IMU_DEMO_H

#include "bsp_board.h"

/* ================================================================
 * API 声明
 * ================================================================ */

/**
 * @brief 初始化姿态传感器自检演示模块
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
bsp_status_t app_imu_demo_init(void);

#endif /* __APP_IMU_DEMO_H */

