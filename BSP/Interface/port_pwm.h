/**
 * @file port_pwm.h
 * @brief PWM 接口层头文件
 * @note 提供统一的 PWM 初始化、启停、占空比/频率设置及 DMA 模式接口。
 */

#ifndef __PORT_PWM_H
#define __PORT_PWM_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    /* 蜂鸣器 PWM 通道 */
    PORT_PWM_BUZZER = 0,
    /* WS2812 触发 PWM 通道 */
    PORT_PWM_WS2812,
    /* LCD 背光 PWM 通道 */
    PORT_PWM_LCD_BL,
    /* PWM 逻辑通道最大值 */
    PORT_PWM_MAX
} port_pwm_id_t;

bsp_status_t port_pwm_init(port_pwm_id_t pwm);
bsp_status_t port_pwm_start(port_pwm_id_t pwm);
bsp_status_t port_pwm_stop(port_pwm_id_t pwm);
bsp_status_t port_pwm_set_duty(port_pwm_id_t pwm, uint16_t duty_permille);
bsp_status_t port_pwm_set_freq(port_pwm_id_t pwm, uint32_t freq_hz);
uint32_t port_pwm_get_arr(port_pwm_id_t pwm);
bsp_status_t port_pwm_dma_start(port_pwm_id_t pwm, void *data, uint16_t len);
bsp_status_t port_pwm_dma_stop(port_pwm_id_t pwm);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_PWM_H */
