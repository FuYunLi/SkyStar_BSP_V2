/**
 * @file app_rs485_demo.h
 * @brief 隔离 RS485 自检演示头文件
 * @note 对标官方出厂 drv_rs485。【未验证】RS485 对端尚未接线实测。
 */

#ifndef __APP_RS485_DEMO_H
#define __APP_RS485_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 RS485 演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_rs485_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RS485_DEMO_H */
