/**
 * @file bsp_backlight.h
 * @brief 屏幕背光控制头文件
 * @note 遵循 SkyStar BSP V2 规范，基于 port_pwm 接口控制屏幕亮度
 */

#ifndef __BSP_BACKLIGHT_H
#define __BSP_BACKLIGHT_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BACKLIGHT_MAX_BRIGHTNESS 100
#define BACKLIGHT_DEFAULT_BRIGHTNESS 50

/**
 * @brief 初始化背光 PWM 并在使能后将亮度设为默认值
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_backlight_init(void);

/**
 * @brief 设置屏幕亮度
 * @param brightness 0-100 阶亮度
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_backlight_set(uint8_t brightness);

/**
 * @brief 获取当前亮度值
 * @return uint8_t 0-100 阶当前亮度
 */
uint8_t bsp_backlight_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BACKLIGHT_H */
