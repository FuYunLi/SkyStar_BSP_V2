/**
 * @file app_lvgl_fs_demo.h
 * @brief LVGL 文件系统集成自检测试演示模块头文件
 */

#ifndef __APP_LVGL_FS_DEMO_H
#define __APP_LVGL_FS_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 文件系统演示模块
 * @retval BSP_OK 初始化成功
 * @retval 其他 错误码
 */
bsp_status_t app_lvgl_fs_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LVGL_FS_DEMO_H */
