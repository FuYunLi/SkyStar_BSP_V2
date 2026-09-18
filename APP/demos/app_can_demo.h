/**
 * @file app_can_demo.h
 * @brief 隔离 CAN 自检演示头文件
 * @note 对标官方出厂 CAN 环回例程。【未验证】总线尚未实测。
 */

#ifndef __APP_CAN_DEMO_H
#define __APP_CAN_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 CAN 演示模块（不自动启动接口，用 can_init 指令启动）
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_can_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CAN_DEMO_H */
