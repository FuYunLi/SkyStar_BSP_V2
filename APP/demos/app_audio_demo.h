/**
 * @file app_audio_demo.h
 * @brief 自检演示模块——板载音频子系统 Shell 自检指令接口
 */

#ifndef __APP_AUDIO_DEMO_H
#define __APP_AUDIO_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化板载音频自检演示模块
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_audio_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_AUDIO_DEMO_H */
