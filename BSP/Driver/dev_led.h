/**
 * @file dev_led.h
 * @brief 核心板板载 LED 驱动头文件
 * @note 提供核心板单色 LED (PB8) 的初始化与电平控制接口。
 */

#ifndef __DEV_LED_H
#define __DEV_LED_H

#include "bsp_board.h"
#include "port_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DEV_LED_OFF = 0,
    DEV_LED_ON
} dev_led_state_t;

bsp_status_t dev_led_init(void);
bsp_status_t dev_led_set(dev_led_state_t state);
bsp_status_t dev_led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_LED_H */
