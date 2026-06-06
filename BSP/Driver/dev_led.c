/**
 * @file dev_led.c
 * @brief 核心板板载 LED 驱动实现
 * @note 实现核心板单色 LED (PB8) 的控制，提供高电平熄灭、低电平点亮的逻辑倒转封装。
 */

#include "dev_led.h"

/**
 * @brief 初始化板载 LED 硬件引脚并置于默认关闭状态
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_led_init(void)
{
    /* 核心板 LED 物理上为低电平点亮，初始化时写入高电平以关闭 */
    return port_gpio_write(PORT_GPIO_LED_CORE, PORT_GPIO_HIGH);
}

/**
 * @brief 设置板载 LED 的开关状态
 * @param state 目标开关状态 (DEV_LED_OFF / DEV_LED_ON)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t dev_led_set(dev_led_state_t state)
{
    if (state != DEV_LED_OFF && state != DEV_LED_ON)
    {
        return BSP_EINVAL;
    }

    /* 极性转换：DEV_LED_ON 映射为引脚输出低电平，DEV_LED_OFF 映射为引脚输出高电平 */
    port_gpio_state_t pin_state = (state == DEV_LED_ON) ? PORT_GPIO_LOW : PORT_GPIO_HIGH;
    return port_gpio_write(PORT_GPIO_LED_CORE, pin_state);
}

/**
 * @brief 翻转板载 LED 的状态
 * @retval BSP_OK 翻转成功
 */
bsp_status_t dev_led_toggle(void)
{
    return port_gpio_toggle(PORT_GPIO_LED_CORE);
}
