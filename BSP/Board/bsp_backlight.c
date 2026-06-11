/**
 * @file bsp_backlight.c
 * @brief 屏幕背光控制源文件
 * @note 依赖 port_pwm 接口层，控制 LCD_BLK 引脚 (TIM10_CH1)
 */

#include "bsp_backlight.h"
#include "port_pwm.h"

/* 使用 port_pwm 中映射的屏幕背光逻辑通道 */
#define BACKLIGHT_PWM_ID PORT_PWM_LCD_BL

/* 记录当前亮度 */
static uint8_t s_current_brightness = BACKLIGHT_DEFAULT_BRIGHTNESS;

/**
 * @brief 初始化背光 PWM 并在使能后将亮度设为默认值
 * @retval bsp_status_t 成功返回 BSP_OK
 * @brief 使用示例：
 * @code
 * bsp_backlight_init();
 * @endcode
 */
bsp_status_t bsp_backlight_init(void)
{
    bsp_status_t status = port_pwm_init(BACKLIGHT_PWM_ID);
    if (status != BSP_OK)
    {
        return status;
    }

    status = port_pwm_start(BACKLIGHT_PWM_ID);
    if (status != BSP_OK)
    {
        return status;
    }

    return bsp_backlight_set(BACKLIGHT_DEFAULT_BRIGHTNESS);
}

/**
 * @brief 设置屏幕亮度
 * @param brightness 0-100 阶亮度
 * @retval bsp_status_t 成功返回 BSP_OK
 * @brief 使用示例：
 * @code
 * bsp_backlight_set(80); // 设置屏幕亮度为 80%
 * @endcode
 */
bsp_status_t bsp_backlight_set(uint8_t brightness)
{
    if (brightness > BACKLIGHT_MAX_BRIGHTNESS)
    {
        brightness = BACKLIGHT_MAX_BRIGHTNESS;
    }

    // 占空比换算为千分比: duty = brightness * 1000 / 100
    uint16_t duty = (uint16_t)((uint32_t)brightness * 1000 / BACKLIGHT_MAX_BRIGHTNESS);
    
    bsp_status_t status = port_pwm_set_duty(BACKLIGHT_PWM_ID, duty);
    if (status == BSP_OK)
    {
        s_current_brightness = brightness;
    }

    return status;
}

/**
 * @brief 获取当前亮度值
 * @return uint8_t 0-100 阶当前亮度
 * @brief 使用示例：
 * @code
 * uint8_t cur = bsp_backlight_get();
 * @endcode
 */
uint8_t bsp_backlight_get(void)
{
    return s_current_brightness;
}
