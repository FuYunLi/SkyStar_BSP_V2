/**
 * @file app_sd_pic_demo.h
 * @brief SD 卡图片显示自检演示头文件
 * @note 对标 RocketPi 25_rocketpi_sd_pic_to_lcd，解析 BMP 文件并逐行
 *       绘制到 ST7789 屏幕。
 */

#ifndef __APP_SD_PIC_DEMO_H
#define __APP_SD_PIC_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SD 卡图片显示演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_sd_pic_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SD_PIC_DEMO_H */
