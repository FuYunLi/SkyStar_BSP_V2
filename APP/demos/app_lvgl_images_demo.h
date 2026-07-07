/**
 * @file app_lvgl_images_demo.h
 * @brief LVGL 编码器控制相册展示自检演示模块头文件
 */

#ifndef __APP_LVGL_IMAGES_DEMO_H
#define __APP_LVGL_IMAGES_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 相册展示演示模块，检索 LittleFS 图像并初始化 UI
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
bsp_status_t app_lvgl_images_demo_init(void);

/**
 * @brief LVGL 相册周期轮询处理函数，负责检测 EC11 编码器与按键交互
 */
void app_lvgl_images_demo_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LVGL_IMAGES_DEMO_H */
