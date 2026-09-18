/**
 * @file dev_hx711.h
 * @brief HX711 24 位称重 ADC 驱动头文件
 * @note 对标 RocketPi HX711 例程与官方出厂 hx711_driver.c。
 *       PB0=DOUT、PB1=SCK（SW7 BIT2 拨至 HX711 位）。
 *       【未验证】外部称重传感器尚未接线实测。
 */

#ifndef __DEV_HX711_H
#define __DEV_HX711_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化 HX711（引脚模式已在 port_gpio_init 配置）
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_hx711_init(void);

/**
 * @brief 读取一次 24 位原始值（阻塞式，最长约 150ms）
 * @param[out] raw 24 位补码原始值
 * @retval BSP_OK 读取成功
 * @retval BSP_EINVAL 参数无效
 * @retval BSP_ETIMEOUT DOUT 长时间不就绪
 */
bsp_status_t dev_hx711_read_raw(int32_t *raw);

/**
 * @brief 设置皮重偏移（原始值域）
 */
bsp_status_t dev_hx711_set_offset(int32_t offset);

/**
 * @brief 设置刻度系数（原始值 → 克的除数）
 */
bsp_status_t dev_hx711_set_scale(float scale);

/**
 * @brief 读取净重（已扣皮重并按刻度换算）
 * @param[out] weight_g 重量（克）
 */
bsp_status_t dev_hx711_read_weight_g(float *weight_g);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_HX711_H */
