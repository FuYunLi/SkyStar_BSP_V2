/**
 * @file dev_stepper.c
 * @brief TMC2209 步进电机驱动实现
 * @note 每个STEP 上升沿走一步（含细分）；速度由脉冲频率决定。
 *       原型实现为"PWM 连续脉冲 + 定时停止"，步数按时间折算，
 *       精确步数计数需 TIM2 DMA 脉冲队列或编码器反馈（扩展项）。
 *       【未验证】步进电机尚未接线实测。
 */

#include "dev_stepper.h"
#include "port_gpio.h"
#include "port_pwm.h"
#include "port_tick.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define STEPPER_PWM_CH        (PORT_PWM_STEPPER_STEP)
#define STEPPER_DUTY_PERMILLE (500U)  /* 50% 方波脉冲 */
#define STEPPER_FREQ_MIN_HZ   (100U)
#define STEPPER_FREQ_MAX_HZ   (8000U)

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化步进电机
 */
bsp_status_t dev_stepper_init(void)
{
    bsp_status_t ret = port_pwm_init(STEPPER_PWM_CH);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 脉冲默认 1kHz/50%，先停发，占空比保持 50% */
    ret = port_pwm_set_freq(STEPPER_PWM_CH, STEPPER_FREQ_MIN_HZ);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = port_pwm_set_duty(STEPPER_PWM_CH, STEPPER_DUTY_PERMILLE);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return port_pwm_stop(STEPPER_PWM_CH);
}

/**
 * @brief 使能或禁用步进驱动
 */
bsp_status_t dev_stepper_enable(bool enable)
{
    /* ENN 低有效：使能 = 拉低 */
    return port_gpio_write(PORT_GPIO_STEPPER_ENN, enable ? PORT_GPIO_LOW : PORT_GPIO_HIGH);
}

/**
 * @brief 设置旋转方向
 */
bsp_status_t dev_stepper_set_dir(bool cw)
{
    return port_gpio_write(PORT_GPIO_STEPPER_DIR, cw ? PORT_GPIO_HIGH : PORT_GPIO_LOW);
}

/**
 * @brief 按指定步进速率移动指定步数（阻塞式原型实现）
 */
bsp_status_t dev_stepper_move_steps(uint32_t steps, uint32_t freq_hz)
{
    if ((steps == 0U) || (freq_hz < STEPPER_FREQ_MIN_HZ) || (freq_hz > STEPPER_FREQ_MAX_HZ))
    {
        return BSP_EINVAL;
    }

    /* 以目标频率输出连续脉冲，按 time = steps / freq 折算时长 */
    bsp_status_t ret = port_pwm_set_freq(STEPPER_PWM_CH, freq_hz);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = port_pwm_start(STEPPER_PWM_CH);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 毫秒定时折算：毫秒数 = steps × 1000 / freq（向上取整） */
    uint32_t duration_ms = ((steps * 1000U) + freq_hz - 1U) / freq_hz;
    port_tick_delay_ms(duration_ms);

    return port_pwm_stop(STEPPER_PWM_CH);
}
