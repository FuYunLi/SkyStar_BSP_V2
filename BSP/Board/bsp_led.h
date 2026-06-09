/**
 * @file bsp_led.h
 * @brief 扩展底板 PCA9555 控制的 LED 驱动头文件
 * @note 提供底板 8 路 LED 的初始化与控制接口，统一使用 bsp_ 前缀和面向过程风格
 */

#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 扩展底板 8 路 LED ID 定义 */
typedef enum
{
    BSP_LED_1 = 0,
    BSP_LED_2,
    BSP_LED_3,
    BSP_LED_4,
    BSP_LED_5,
    BSP_LED_6,
    BSP_LED_7,
    BSP_LED_8,
    BSP_LED_MAX
} bsp_led_id_t;

/* LED 状态定义 */
typedef enum
{
    BSP_LED_OFF = 0,
    BSP_LED_ON
} bsp_led_state_t;

bsp_status_t bsp_led_init(void);
bsp_status_t bsp_led_set(bsp_led_id_t led_id, bsp_led_state_t state);
bsp_status_t bsp_led_toggle(bsp_led_id_t led_id);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LED_H */
