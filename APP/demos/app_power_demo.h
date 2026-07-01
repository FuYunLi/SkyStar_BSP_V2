/**
 * @file app_power_demo.h
 * @brief 功率与电量监测自检 Demo 头文件
 */

#ifndef __APP_POWER_DEMO_H
#define __APP_POWER_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化功率监测自检 Demo 并注册 Shell 指令
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_power_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_POWER_DEMO_H */
