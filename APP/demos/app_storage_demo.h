/**
 * @file app_storage_demo.h
 * @brief 板载存储服务演示与测试模块头文件
 */

#ifndef __APP_STORAGE_DEMO_H
#define __APP_STORAGE_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化板载存储服务演示模块
 * @retval bsp_status_t 执行结果，成功返回 BSP_OK
 */
bsp_status_t app_storage_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_STORAGE_DEMO_H */
