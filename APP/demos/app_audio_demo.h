/**
 * @file app_audio_demo.h
 * @brief 自检演示模块——音频子系统 Shell 自检指令头文件
 * @note  阶段八按里程碑逐步扩充：M30 提供总线仲裁切换验证。
 */

#ifndef __APP_AUDIO_DEMO_H
#define __APP_AUDIO_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化音频子系统演示模块（注册 Shell 命令）
 * @return bsp_status_t 执行结果
 */
bsp_status_t app_audio_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_AUDIO_DEMO_H */
