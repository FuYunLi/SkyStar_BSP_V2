/**
 * @file app_mic_demo.h
 * @brief 麦克风录音自检演示头文件
 * @note 对标官方出厂 drv_mic。【未验证】录音链路尚未上板实测。
 */

#ifndef __APP_MIC_DEMO_H
#define __APP_MIC_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化麦克风录音演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_mic_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MIC_DEMO_H */
