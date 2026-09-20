/**
 * @file bsp_audio.h
 * @brief 板级音频播放服务头文件
 * @note  编排 bsp_bus 总线仲裁、port_i2s 流式发送、dev_es8388/HT6872 与
 *        bsp_file 文件读取，提供 WAV 播放的板级业务封装。
 *        当前约束：16-bit PCM、采样率 44.1kHz（I2S2 固定配置）、单/立体声。
 */

#ifndef __BSP_AUDIO_H
#define __BSP_AUDIO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放指定 WAV 文件（非阻塞，后台填充双缓冲）
 * @note  自动完成总线仲裁接管、I2S2 初始化、codec 启动与功放使能。
 * @param path WAV 文件路径（"0:/..." 指向 TF 卡 FatFS 分区）
 * @return bsp_status_t 执行结果
 *         - BSP_OK 播放已启动
 *         - BSP_BUSY 正在播放中
 *         - BSP_EINVAL 路径为空或 WAV 格式不支持
 *         - BSP_ENODEV 文件不存在
 *         - 其他 底层错误
 */
bsp_status_t bsp_audio_play(const char *path);

/**
 * @brief 停止播放（幂等：未播放时直接返回成功）
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_audio_stop(void);

/**
 * @brief 查询播放状态
 * @return true 正在播放
 */
bool bsp_audio_is_playing(void);

#endif /* __BSP_AUDIO_H */
