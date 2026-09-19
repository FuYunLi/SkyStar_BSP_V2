/**
 * @file dev_ht6872.h
 * @brief HT6872 D类音频功放使能控制驱动头文件
 * @note  使能脚经 PCA9555 IO 扩展器输出（对应底板拨码开关 BIT1，
 *        板上 1K 下拉确保复位/浮空时默认静音，软件控制优先级高于拨码）。
 */

#ifndef __DEV_HT6872_H
#define __DEV_HT6872_H

#include "bsp_board.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化功放使能通道（配置 PCA9555 对应位为输出，默认保持静音）
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_ht6872_init(void);

/**
 * @brief 功放使能控制
 * @param en true 使能（出声）/ false 关闭（静音）
 * @return bsp_status_t 执行结果
 */
bsp_status_t dev_ht6872_enable(bool en);

#endif /* __DEV_HT6872_H */
