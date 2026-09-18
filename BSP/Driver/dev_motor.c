/**
 * @file dev_motor.c
 * @brief AT8236 直流电机驱动实现
 * @note AT8236 控制真值表（IN1/IN2 PWM-PWM 模式）：
 *       IN1=PWM, IN2=0 → 正转，速度=PWM 占空比
 *       IN1=0,   IN2=PWM → 反转
 *       IN1=IN2=0 → 滑行（输出高阻）
 *       IN1=IN2=1 → 刹车（低边导通）
 */

#include "dev_motor.h"
#include "port_pwm.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define MOTOR_SPEED_MAX (1000) /* 速度千分比上限 */

/* 电机1 的 IN1/IN2 通道映射 */
static const port_pwm_id_t s_motor_ch_map[DEV_MOTOR_MAX][2] =
{
    [DEV_MOTOR_1] = {PORT_PWM_MOTOR1_IN1, PORT_PWM_MOTOR1_IN2}
};

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 通道越界与参数合法性检查
 */
static bsp_status_t s_motor_check(dev_motor_id_t id)
{
    if (id >= DEV_MOTOR_MAX)
    {
        return BSP_EINVAL;
    }

    return BSP_OK;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化指定电机通道，初始停止状态
 */
bsp_status_t dev_motor_init(dev_motor_id_t id)
{
    bsp_status_t ret = s_motor_check(id);
    if (ret != BSP_OK)
    {
        return ret;
    }

    for (uint32_t ch = 0U; ch < 2U; ch++)
    {
        ret = port_pwm_init(s_motor_ch_map[id][ch]);
        if (ret != BSP_OK)
        {
            return ret;
        }

        ret = port_pwm_set_duty(s_motor_ch_map[id][ch], 0U);
        if (ret != BSP_OK)
        {
            return ret;
        }

        ret = port_pwm_start(s_motor_ch_map[id][ch]);
        if (ret != BSP_OK)
        {
            return ret;
        }
    }

    return BSP_OK;
}

/**
 * @brief 设置电机转速与方向
 */
bsp_status_t dev_motor_set_speed(dev_motor_id_t id, int16_t speed_permille)
{
    bsp_status_t ret = s_motor_check(id);
    if (ret != BSP_OK)
    {
        return ret;
    }

    if (speed_permille > MOTOR_SPEED_MAX)
    {
        speed_permille = MOTOR_SPEED_MAX;
    }
    if (speed_permille < -MOTOR_SPEED_MAX)
    {
        speed_permille = -MOTOR_SPEED_MAX;
    }

    uint16_t duty = (uint16_t)((speed_permille >= 0) ? speed_permille : -speed_permille);

    if (speed_permille >= 0)
    {
        /* 正转：IN1 输出 PWM，IN2 拉低 */
        ret = port_pwm_set_duty(s_motor_ch_map[id][0], duty);
        if (ret != BSP_OK)
        {
            return ret;
        }
        return port_pwm_set_duty(s_motor_ch_map[id][1], 0U);
    }

    /* 反转：IN1 拉低，IN2 输出 PWM */
    ret = port_pwm_set_duty(s_motor_ch_map[id][0], 0U);
    if (ret != BSP_OK)
    {
        return ret;
    }
    return port_pwm_set_duty(s_motor_ch_map[id][1], duty);
}

/**
 * @brief 停止电机（滑行）
 */
bsp_status_t dev_motor_stop(dev_motor_id_t id)
{
    bsp_status_t ret = s_motor_check(id);
    if (ret != BSP_OK)
    {
        return ret;
    }

    (void)port_pwm_set_duty(s_motor_ch_map[id][0], 0U);
    (void)port_pwm_set_duty(s_motor_ch_map[id][1], 0U);

    return BSP_OK;
}

/**
 * @brief 刹车
 */
bsp_status_t dev_motor_brake(dev_motor_id_t id)
{
    bsp_status_t ret = s_motor_check(id);
    if (ret != BSP_OK)
    {
        return ret;
    }

    (void)port_pwm_set_duty(s_motor_ch_map[id][0], 1000U);
    (void)port_pwm_set_duty(s_motor_ch_map[id][1], 1000U);

    return BSP_OK;
}
