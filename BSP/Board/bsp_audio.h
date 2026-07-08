/**
 * @file bsp_audio.h
 * @brief 板级音频子系统服务头文件
 * @note 整合总线仲裁、I2S 物理传输与 ES8388 编解码芯片控制，提供上层音频播放 API
 */

#ifndef __BSP_AUDIO_H
#define __BSP_AUDIO_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化板级音频子系统
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_init(void);

/**
 * @brief  播放指定频率和时长的单音（正弦波合成，非阻塞）
 * @param  frequency 单音频率 (Hz)
 * @param  duration_ms 持续时间 (ms)
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_play_tone(uint32_t frequency, uint32_t duration_ms);

/**
 * @brief  播放音频文件（支持 WAV 格式，非阻塞）
 * @param  filepath 文件物理路径（例如 "0:/music.wav"）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_play_file(const char *filepath);

/**
 * @brief  停止音频播放，释放音频总线
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_stop(void);

/**
 * @brief  设置播放音量
 * @param  volume 音量百分比 (0 - 100)
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_set_volume(uint8_t volume);

/**
 * @brief  静音音频输出
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_mute(void);

/**
 * @brief  取消静音音频输出
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_audio_unmute(void);

/**
 * @brief  音频服务轮询更新任务（需在主循环中以非阻塞方式频繁调用）
 */
void bsp_audio_update(void);

/**
 * @brief  查询音频当前是否正在播放
 * @retval bool 正在播放返回 true
 */
bool bsp_audio_is_playing(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_AUDIO_H */
