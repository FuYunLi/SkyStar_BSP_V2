/**
 * @file app_crc_demo.h
 * @brief CRC 校验自检演示头文件
 * @note 对标 RocketPi 40_rocketpi_crc，演示软件 CRC-32 的位级实现
 *       与标准测试向量验证。
 */

#ifndef __APP_CRC_DEMO_H
#define __APP_CRC_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 CRC 校验演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_crc_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CRC_DEMO_H */
