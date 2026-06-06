/**
 * @file dev_buzzer.h
 * @brief 无源蜂鸣器驱动头文件
 * @note 提供无源蜂鸣器 (PA6) 的初始化、开/关、占空比音量及频率频率控制接口。
 */

#ifndef __DEV_BUZZER_H
#define __DEV_BUZZER_H

#include "bsp_board.h"
#include <stdint.h>

/* 定义蜂鸣器类型宏 */
#define BUZZER_PASSIVE 0
#define BUZZER_ACTIVE  1

/* 【配置】当前使用的类型 (二选一) */
#define BUZZER_TYPE BUZZER_PASSIVE
// #define BUZZER_TYPE BUZZER_ACTIVE

#ifdef __cplusplus
extern "C" {
#endif

bsp_status_t dev_buzzer_init(void);
bsp_status_t dev_buzzer_on(void);
bsp_status_t dev_buzzer_off(void);

#if BUZZER_TYPE == BUZZER_PASSIVE
bsp_status_t dev_buzzer_set_volume(uint8_t volume);
bsp_status_t dev_buzzer_set_freq(uint16_t freq);
bsp_status_t dev_buzzer_tone(uint16_t freq, uint8_t volume);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DEV_BUZZER_H */
