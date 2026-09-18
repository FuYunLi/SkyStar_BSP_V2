/**
 * @file dev_ds18b20.h
 * @brief DS18B20 数字温度传感器驱动头文件
 * @note 单总线挂载于 PA8 网络（soft_onewire 承载位时序），
 *       默认跳过 ROM 寻址（单从机场景）。
 *       【未验证】外部传感器尚未接线实测。
 */

#ifndef __DEV_DS18B20_H
#define __DEV_DS18B20_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化单总线引脚
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_ds18b20_init(void);

/**
 * @brief 启动一次温度转换并读取结果（阻塞式，12 位转换约 750ms）
 * @param[out] temp_deci_c 温度回传，单位 0.1°C
 * @retval BSP_OK 读取成功
 * @retval BSP_ENODEV 总线无应答
 * @retval BSP_ERROR 校验和错误
 */
bsp_status_t dev_ds18b20_read_temp(int16_t *temp_deci_c);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_DS18B20_H */
