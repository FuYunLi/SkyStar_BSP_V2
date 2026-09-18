/**
 * @file bsp_mic.h
 * @brief 板级麦克风录音服务层头文件
 * @note 对标官方出厂 drv_mic。ES8388 ADC → I2S2 接收（DMA）→
 *       WAV 落盘（FatFS）。录音时长在启动时确定，WAV 头按最终
 *       尺寸一次写全，数据块随后顺序写入。
 *       【未验证】录音链路尚未上板实测。
 */

#ifndef __BSP_MIC_H
#define __BSP_MIC_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 开始录音（异步，非阻塞返回）
 * @param path 输出 WAV 路径（"0:/..." 指向 FatFS）
 * @param seconds 录音时长（秒，1-60）
 * @param sample_rate 采样率（8000-48000，建议 16000）
 * @retval BSP_OK 已开始录音
 * @retval BSP_BUSY 正在录音
 */
bsp_status_t bsp_mic_record(const char *path, uint32_t seconds, uint32_t sample_rate);

/**
 * @brief 停止录音并落盘收尾
 */
bsp_status_t bsp_mic_stop(void);

/**
 * @brief 录音泵：搬运 DMA 数据到文件
 * @note 须在主循环（app_main_process）中周期调用
 */
void bsp_mic_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_MIC_H */
