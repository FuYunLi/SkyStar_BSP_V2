/**
 * @file dev_es8388.h
 * @brief ES8388 音频编解码器驱动头文件
 * @note  I2C 控制通道驱动（数据面由 I2S 承担）。寄存器序列参照 RT-Thread
 *        官方 bsp/stm32/stm32f407-rt-spark drv_es8388（Apache-2.0），
 *        codec 配置为 I2S 从机模式，本板 I2S2 主机提供时钟。
 */

#ifndef __DEV_ES8388_H
#define __DEV_ES8388_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * ES8388 寄存器空间（0x00 - 0x34）
 * ================================================================ */

#define ES8388_CONTROL1         0x00
#define ES8388_CONTROL2         0x01
#define ES8388_CHIPPOWER        0x02
#define ES8388_ADCPOWER         0x03
#define ES8388_DACPOWER         0x04
#define ES8388_CHIPLOPOW1       0x05
#define ES8388_CHIPLOPOW2       0x06
#define ES8388_ANAVOLMANAG      0x07
#define ES8388_MASTERMODE       0x08
#define ES8388_ADCCONTROL1      0x09
#define ES8388_ADCCONTROL2      0x0A
#define ES8388_ADCCONTROL3      0x0B
#define ES8388_ADCCONTROL4      0x0C
#define ES8388_ADCCONTROL5      0x0D
#define ES8388_ADCCONTROL6      0x0E
#define ES8388_ADCCONTROL7      0x0F
#define ES8388_ADCCONTROL8      0x10
#define ES8388_ADCCONTROL9      0x11
#define ES8388_ADCCONTROL10     0x12
#define ES8388_ADCCONTROL11     0x13
#define ES8388_ADCCONTROL12     0x14
#define ES8388_ADCCONTROL13     0x15
#define ES8388_ADCCONTROL14     0x16
#define ES8388_DACCONTROL1      0x17
#define ES8388_DACCONTROL2      0x18
#define ES8388_DACCONTROL3      0x19
#define ES8388_DACCONTROL4      0x1A
#define ES8388_DACCONTROL5      0x1B
#define ES8388_DACCONTROL6      0x1C
#define ES8388_DACCONTROL7      0x1D
#define ES8388_DACCONTROL8      0x1E
#define ES8388_DACCONTROL9      0x1F
#define ES8388_DACCONTROL10     0x20
#define ES8388_DACCONTROL11     0x21
#define ES8388_DACCONTROL12     0x22
#define ES8388_DACCONTROL13     0x23
#define ES8388_DACCONTROL14     0x24
#define ES8388_DACCONTROL15     0x25
#define ES8388_DACCONTROL16     0x26
#define ES8388_DACCONTROL17     0x27
#define ES8388_DACCONTROL18     0x28
#define ES8388_DACCONTROL19     0x29
#define ES8388_DACCONTROL20     0x2A
#define ES8388_DACCONTROL21     0x2B
#define ES8388_DACCONTROL22     0x2C
#define ES8388_DACCONTROL23     0x2D
#define ES8388_DACCONTROL24     0x2E
#define ES8388_DACCONTROL25     0x2F
#define ES8388_DACCONTROL26     0x30
#define ES8388_DACCONTROL27     0x31
#define ES8388_DACCONTROL28     0x32
#define ES8388_DACCONTROL29     0x33
#define ES8388_DACCONTROL30     0x34

/* ================================================================
 * API 声明
 * ================================================================ */

/**
 * @brief 初始化 ES8388（软复位 + 完整上电序列，DAC 通路使能、ADC 上电）
 * @note  需先经 bsp_bus 仲裁器取得 I2S2 归属，保证芯片供电与 I2C 可达。
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_init(void);

/**
 * @brief 启动 DAC 播放通路（使能 LOUT/ROUT 输出并解除静音）
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_start_dac(void);

/**
 * @brief 停止 DAC 播放通路（静音并关闭输出）
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_stop_dac(void);

/**
 * @brief 设置 DAC 音量
 * @param vol_0_100 音量 0-100（0 最小，100 最大 0dB）
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_set_dac_volume(uint8_t vol_0_100);

/**
 * @brief 读取当前 DAC 音量设置（0-100）
 * @param vol_0_100 存储音量值的指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_get_dac_volume(uint8_t *vol_0_100);

/**
 * @brief 设置 DAC 数字静音
 * @param en true 静音 / false 解除
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_set_mute(bool en);

/**
 * @brief 读回指定寄存器值（自检/调试用）
 * @param reg    寄存器地址（0x00-0x34）
 * @param val    存储读回值的指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_es8388_read_reg(uint8_t reg, uint8_t *val);

#endif /* __DEV_ES8388_H */
