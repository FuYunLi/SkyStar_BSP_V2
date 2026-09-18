/**
 * @file bsp_audio.h
 * @brief 板级音频服务层头文件
 * @note 提供基于 ES8388 + I2S2 的 WAV 播放黑盒接口。双缓冲调度：
 *       DMA 完成回调仅置交换标志（ISR 最小化），缓冲区回填在
 *       bsp_audio_process（主上下文）中完成。
 *       【未验证】音频链路尚未上板实测。硬件前置条件：
 *       SW7 BIT3 拨至 I2S2（ES8388 数据通道），BIT1 拨至功放开启。
 */

#ifndef __BSP_AUDIO_H
#define __BSP_AUDIO_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化音频服务（ES8388 寄存器初始化 + 存活检查）
 * @note 仅完成编解码器准备，I2S 接口在每次播放时按采样率切换建立，
 *       停止后自动恢复 SPI2（W25Q/IMU 可用）。
 * @retval BSP_OK 初始化成功
 * @retval BSP_ENODEV ES8388 无应答
 */
bsp_status_t bsp_audio_init(void);

/**
 * @brief 播放 SD 卡上的 WAV 文件（异步，非阻塞返回）
 * @param path 文件路径（"0:/..." 指向 FatFS）
 * @note 支持未压缩 16 位 PCM（单声道自动扩展为双声道），
 *       采样率 8k-48k。
 * @retval BSP_OK 已开始播放
 * @retval BSP_BUSY 正在播放中
 * @retval BSP_EINVAL 参数无效
 * @retval BSP_ERROR 文件打开或 WAV 格式不支持
 */
bsp_status_t bsp_audio_play_wav(const char *path);

/**
 * @brief 停止播放并恢复 SPI2
 */
bsp_status_t bsp_audio_stop(void);

/**
 * @brief 设置播放音量
 * @param volume 音量 (0-100)
 */
bsp_status_t bsp_audio_set_volume(uint8_t volume);

/**
 * @brief 播放泵：回填缓冲区并驱动下一块 DMA
 * @note 须在主循环（app_main_process）中周期调用
 */
void bsp_audio_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_AUDIO_H */
