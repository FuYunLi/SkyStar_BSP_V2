/**
 * @file port_gpio.h
 * @brief GPIO 接口层头文件
 * @note 提供统一的 GPIO 读写、翻转及 EXTI 中断配置接口，隔离物理引脚与上层业务逻辑。
 */

#ifndef __PORT_GPIO_H
#define __PORT_GPIO_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    /* 核心板上的 LED (PB8) */
    PORT_GPIO_LED_CORE = 0,
    /* 核心板上的按键 (PA0/WKUP) */
    PORT_GPIO_KEY1,
    /* 核心板上的按键 (PE8) */
    PORT_GPIO_KEY2,
    /* 核心板上的按键 (PC13) */
    PORT_GPIO_KEY3,
    /* 蜂鸣器 (PA6) */
    PORT_GPIO_BUZZER,
    /* LCD CS (PE14) */
    PORT_GPIO_LCD_CS,
    /* LCD DC (PD14) */
    PORT_GPIO_LCD_DC,
    /* LCD RST (PE1) */
    PORT_GPIO_LCD_RST,
    /* LCD TOUCH 软件i2c 时钟 (PD10) */
    PORT_GPIO_TOUCH_SCL,
    /* LCD TOUCH 软件i2c 数据 (PE13) */
    PORT_GPIO_TOUCH_SDA,
    /* LCD TOUCH 中断 (PE2) */
    PORT_GPIO_TOUCH_INT,
    /* W25Q CS (PE4) */
    PORT_GPIO_W25Q_CS,
    /* SD3078 中断 (PE3) */
    PORT_GPIO_SD3078_INT,
    /* 逻辑引脚最大值 */
    PORT_GPIO_MAX
} port_gpio_id_t;

typedef enum
{
    /* 低电平 */
    PORT_GPIO_LOW = 0,
    /* 高电平 */
    PORT_GPIO_HIGH
} port_gpio_state_t;

typedef enum
{
    /* 上升沿触发 */
    PORT_EXTI_TRIGGER_RISING = 0,
    /* 下降沿触发 */
    PORT_EXTI_TRIGGER_FALLING,
    /* 双边沿触发 */
    PORT_EXTI_TRIGGER_BOTH
} port_exti_trigger_t;

typedef void (*port_exti_callback_t)(void);

bsp_status_t port_gpio_init(void);
bsp_status_t port_gpio_read(port_gpio_id_t pin_id, port_gpio_state_t *state);
bsp_status_t port_gpio_write(port_gpio_id_t pin_id, port_gpio_state_t state);
bsp_status_t port_gpio_toggle(port_gpio_id_t pin_id);
bsp_status_t port_gpio_exti_init(port_gpio_id_t pin_id, port_exti_trigger_t trigger, port_exti_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_GPIO_H */
