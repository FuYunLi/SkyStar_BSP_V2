/**
 * @file app_audio_demo.h
 * @brief SD 卡 WAV 播放自检演示头文件
 * @note 对标 RocketPi sd_audio_to_i2s（简化版：16 位 PCM 直读，
 *       无解码库）。【未验证】音频链路尚未上板实测。
 */

#ifndef __APP_AUDIO_DEMO_H
#define __APP_AUDIO_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 WAV 播放演示模块
 * @retval BSP_OK 初始化成功
 * @retval BSP_ENODEV ES8388 无应答
 */
bsp_status_t app_audio_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_AUDIO_DEMO_H */
