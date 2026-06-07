/**
 * @file dev_led.c
 * @brief 核心板板载 LED 驱动实现
 * @note 实现多路 LED 的配置隔离，提供极性映射封装。
 */

#include "dev_led.h"

/* 定义 LED 硬件配置结构体 */
typedef struct
{
    port_gpio_id_t pin_id;
    uint8_t        active_level;
} dev_led_map_t;

/* 硬件配置表 */
static const dev_led_map_t led_mapping[] = {
    /* 核心板上的 LED (PB8)，硬件为低电平点亮 */
    [LED_CORE] = { PORT_GPIO_LED_CORE, 0 }
};

/**
 * @brief 初始化板载 LED 硬件引脚并置于默认关闭状态
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_led_init(void)
{
    for (uint8_t i = 0; i < DEV_LED_MAX; i++)
    {
        /* 初始化时输出关闭状态对应的物理电平 */
        port_gpio_state_t default_state = led_mapping[i].active_level ? PORT_GPIO_LOW : PORT_GPIO_HIGH;
        port_gpio_write(led_mapping[i].pin_id, default_state);
    }
    return BSP_OK;
}

/**
 * @brief 设置板载 LED 的开关状态
 * @param led_id LED ID
 * @param state  目标开关状态 (DEV_LED_OFF / DEV_LED_ON)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_led_set(dev_led_id_t led_id, dev_led_state_t state)
{
    if (led_id >= DEV_LED_MAX || (state != DEV_LED_OFF && state != DEV_LED_ON))
    {
        return BSP_EINVAL;
    }

    /* 极性转换：根据 active_level 算出实际应该输出的物理高低电平 */
    port_gpio_state_t pin_state;
    if (state == DEV_LED_ON)
    {
        pin_state = led_mapping[led_id].active_level ? PORT_GPIO_HIGH : PORT_GPIO_LOW;
    }
    else
    {
        pin_state = led_mapping[led_id].active_level ? PORT_GPIO_LOW : PORT_GPIO_HIGH;
    }
    
    return port_gpio_write(led_mapping[led_id].pin_id, pin_state);
}

/**
 * @brief 翻转板载 LED 的状态
 * @param led_id LED ID
 * @retval BSP_OK 翻转成功
 */
bsp_status_t dev_led_toggle(dev_led_id_t led_id)
{
    if (led_id >= DEV_LED_MAX)
    {
        return BSP_EINVAL;
    }

    return port_gpio_toggle(led_mapping[led_id].pin_id);
}
