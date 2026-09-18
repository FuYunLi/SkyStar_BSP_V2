/**
 * @file dev_sg90.c
 * @brief SG90 舵机驱动实现
 * @note 控制原理：标准舵机以 50Hz 周期接收脉冲，脉宽 0.5-2.5ms 线性
 *       对应 0-180° 角度。本模块将角度换算为 TIM12 的 CCR 微秒值
 *       （时基 1MHz，见 port_pwm 自持初始化）。
 */

#include "dev_sg90.h"
#include "port_pwm.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define SG90_MIN_PULSE_US (500U)  /* 0° 对应脉宽 */
#define SG90_MAX_PULSE_US (2500U) /* 180° 对应脉宽 */
#define SG90_MAX_ANGLE    (180U)  /* 行程上限 */

/* 逻辑舵机到 PWM 通道映射 */
static const port_pwm_id_t s_sg90_pwm_map[DEV_SG90_MAX] =
{
    [DEV_SG90_1] = PORT_PWM_SERVO1,
    [DEV_SG90_2] = PORT_PWM_SERVO2
};

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化指定舵机通道并归中 90°
 */
bsp_status_t dev_sg90_init(dev_sg90_id_t id)
{
    if (id >= DEV_SG90_MAX)
    {
        return BSP_EINVAL;
    }

    bsp_status_t ret = port_pwm_init(s_sg90_pwm_map[id]);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = port_pwm_start(s_sg90_pwm_map[id]);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return dev_sg90_set_angle(id, SG90_MAX_ANGLE / 2U);
}

/**
 * @brief 设置舵机角度
 */
bsp_status_t dev_sg90_set_angle(dev_sg90_id_t id, uint8_t angle)
{
    if (id >= DEV_SG90_MAX)
    {
        return BSP_EINVAL;
    }

    if (angle > SG90_MAX_ANGLE)
    {
        angle = SG90_MAX_ANGLE;
    }

    /* 角度线性映射到脉宽：500µs + angle × (2000µs / 180°)，
     * 先乘后除保精度，结果落在 500-2500 之内 */
    uint16_t pulse_us = (uint16_t)(SG90_MIN_PULSE_US + ((uint32_t)angle * (SG90_MAX_PULSE_US - SG90_MIN_PULSE_US)) / SG90_MAX_ANGLE);

    return port_pwm_set_pulse_us(s_sg90_pwm_map[id], pulse_us);
}
