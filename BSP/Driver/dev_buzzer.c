/**
 * @file dev_buzzer.c
 * @brief 有源/无源蜂鸣器驱动实现
 * @note 封装通用 PWM 接口层 (port_pwm) 以控制无源蜂鸣器 (PA6) 的工作状态。
 */

#include "dev_buzzer.h"
#include "port_pwm.h"
#include "port_gpio.h"

#if BUZZER_TYPE == BUZZER_PASSIVE
/* 定义音量与占空比的最大/默认映射值 */
#define BUZZER_MAX_VOLUME   100U
#define BUZZER_MAX_DUTY     500U   /* 限制最大占空比为 50%，防止持续直流输出导致器件过热 */
#define BUZZER_DEFAULT_FREQ 2000U  /* 默认频率 2kHz */
#define BUZZER_DEFAULT_VOL  50U    /* 默认音量 50% */

static uint16_t s_buzzer_freq = BUZZER_DEFAULT_FREQ;
static uint8_t s_buzzer_vol = BUZZER_DEFAULT_VOL;
#endif

/**
 * @brief 初始化无源蜂鸣器驱动，并默认置于关闭状态
 * @retval BSP_OK 初始化成功
 */
bsp_status_t dev_buzzer_init(void)
{
#if BUZZER_TYPE == BUZZER_PASSIVE
    bsp_status_t ret = port_pwm_init(PORT_PWM_BUZZER);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = port_pwm_set_freq(PORT_PWM_BUZZER, s_buzzer_freq);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 线性映射音量至占空比 (千分比) */
    uint16_t duty = s_buzzer_vol * 5;
    ret = port_pwm_set_duty(PORT_PWM_BUZZER, duty);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return port_pwm_stop(PORT_PWM_BUZZER);
#else
    /* 有源蜂鸣器 GPIO 已在 port_gpio_init 中初始化，此处只需确保关闭 */
    return dev_buzzer_off();
#endif
}

/**
 * @brief 开启蜂鸣器发声
 * @retval BSP_OK 启动成功
 */
bsp_status_t dev_buzzer_on(void)
{
#if BUZZER_TYPE == BUZZER_PASSIVE
    return port_pwm_start(PORT_PWM_BUZZER);
#else
    return port_gpio_write(PORT_GPIO_BUZZER, PORT_GPIO_HIGH);
#endif
}

/**
 * @brief 关闭蜂鸣器发声
 * @retval BSP_OK 停止成功
 */
bsp_status_t dev_buzzer_off(void)
{
#if BUZZER_TYPE == BUZZER_PASSIVE
    return port_pwm_stop(PORT_PWM_BUZZER);
#else
    return port_gpio_write(PORT_GPIO_BUZZER, PORT_GPIO_LOW);
#endif
}

#if BUZZER_TYPE == BUZZER_PASSIVE

/**
 * @brief 设置蜂鸣器的音量
 * @note 线性映射音量 0-100 至 0-50% 占空比。
 * @param volume 音量值 (0 - 100)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数超出范围
 */
bsp_status_t dev_buzzer_set_volume(uint8_t volume)
{
    if (volume > BUZZER_MAX_VOLUME)
    {
        volume = BUZZER_MAX_VOLUME;
    }

    s_buzzer_vol = volume;
    uint16_t duty = volume * 5;

    return port_pwm_set_duty(PORT_PWM_BUZZER, duty);
}

/**
 * @brief 设置蜂鸣器的发声频率
 * @param freq 目标频率（Hz）
 * @retval BSP_OK 设置成功
 * @retval 其他 频率设置失败
 */
bsp_status_t dev_buzzer_set_freq(uint16_t freq)
{
    if (freq == 0)
    {
        return BSP_EINVAL;
    }

    s_buzzer_freq = freq;
    bsp_status_t ret = port_pwm_set_freq(PORT_PWM_BUZZER, freq);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 重设频率后，需重新应用占空比以保持音量恒定 */
    uint16_t duty = s_buzzer_vol * 5;
    return port_pwm_set_duty(PORT_PWM_BUZZER, duty);
}

/**
 * @brief 触发蜂鸣器按指定频率与音量鸣叫
 * @param freq 目标发声频率（Hz）
 * @param volume 音量值 (0 - 100)
 * @retval BSP_OK 设置并发声成功
 *
 * 示例：
 *   dev_buzzer_tone(2000, 50); // 以 2kHz 频率、50% 音量鸣叫
 */
bsp_status_t dev_buzzer_tone(uint16_t freq, uint8_t volume)
{
    if (freq == 0)
    {
        return BSP_EINVAL;
    }

    if (volume > BUZZER_MAX_VOLUME)
    {
        volume = BUZZER_MAX_VOLUME;
    }

    s_buzzer_freq = freq;
    s_buzzer_vol = volume;

    bsp_status_t ret = port_pwm_set_freq(PORT_PWM_BUZZER, freq);
    if (ret != BSP_OK)
    {
        return ret;
    }

    uint16_t duty = volume * 5;
    ret = port_pwm_set_duty(PORT_PWM_BUZZER, duty);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return port_pwm_start(PORT_PWM_BUZZER);
}

#endif
