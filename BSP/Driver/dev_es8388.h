/**
 * @file dev_es8388.h
 * @brief ES8388 音频编解码器驱动头文件
 * @note 对标 RocketPi 音频线，寄存器上电序列对照立创官方出厂例程
 *       （SkyStar-fdb，RT-Thread 组件）校准。ES8388 为 I2C 控制
 *       （7 位地址 0x10）+ I2S 数据的编解码器，本驱动只管寄存器，
 *       音频数据通路由 port_i2s 承载。
 *       【未验证】音频链路尚未上板实测。
 */

#ifndef __DEV_ES8388_H
#define __DEV_ES8388_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 按官方上电序列初始化 ES8388（DAC 播放路径，ADC 路径下电）
 * @note 含软复位、静音、I2S 从机模式（16 位/飞利浦/256fs）、
 *       模拟输出下电等步骤；I2S 时钟稳定后再调 dev_es8388_start。
 * @retval BSP_OK 初始化成功
 * @retval BSP_ERROR I2C 访问失败
 */
bsp_status_t dev_es8388_init(void);

/**
 * @brief 启动 DAC 播放路径（上电解静音，需 I2S 时钟已输出）
 */
bsp_status_t dev_es8388_start(void);

/**
 * @brief 停止 DAC 播放路径（静音下电）
 */
bsp_status_t dev_es8388_stop(void);

/**
 * @brief 启动 ADC 录音路径（板载差分麦克风，需 I2S 时钟已输出）
 */
bsp_status_t dev_es8388_start_adc(void);

/**
 * @brief 停止 ADC 录音路径
 */
bsp_status_t dev_es8388_stop_adc(void);

/**
 * @brief 设置 DAC 音量
 * @param volume 音量 (0-100)，越界自动钳位
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_es8388_set_volume(uint8_t volume);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_ES8388_H */
