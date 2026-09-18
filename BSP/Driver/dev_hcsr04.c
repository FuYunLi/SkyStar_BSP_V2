/**
 * @file dev_hcsr04.c
 * @brief HC-SR04 超声波测距驱动实现
 * @note 时序测量基于 DWT 周期计数器（168MHz），软件轮询回波沿——
 *       实现最直观；追求不阻塞可演进为 TIM 输入捕获（参照 RocketPi
 *       原版），此处先立坐标。本函数阻塞最长约 60ms，仅限主上下文调用。
 */

#include "dev_hcsr04.h"
#include "port_gpio.h"
#include "port_dwt.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define HCSR04_TRIG_PULSE_US   (20U)     /* 触发脉冲宽度（≥10µs 即可） */
#define HCSR04_ECHO_TIMEOUT_US (30000U)  /* 回波超时：343m/s 下约 5m 折返 */
#define HCSR04_CPU_FREQ_MHZ    (168U)    /* DWT 计数频率（MHz） */

/* 声速 343m/s = 0.343mm/µs，往返距离除以 2：mm = echo_us × 343 / 2000 */
#define HCSR04_MM_PER_US_NUM   (343U)
#define HCSR04_MM_PER_US_DEN   (2000U)

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 等待回波脚到达指定电平，带超时
 * @param level 目标电平
 * @param timeout_us 超时时间（微秒）
 * @param[out] elapsed_us 从进入函数到电平命中的时长回传
 * @retval BSP_OK 命中；BSP_ETIMEOUT 超时
 */
static bsp_status_t s_hcsr04_wait_level(port_gpio_state_t level, uint32_t timeout_us, uint32_t *elapsed_us)
{
    uint32_t start = port_dwt_get_cycles();
    port_gpio_state_t cur = level;

    do
    {
        (void)port_gpio_read(PORT_GPIO_HCSR04_ECHO, &cur);
        *elapsed_us = (port_dwt_get_cycles() - start) / HCSR04_CPU_FREQ_MHZ;

        if (*elapsed_us > timeout_us)
        {
            return BSP_ETIMEOUT;
        }
    } while (cur != level);

    return BSP_OK;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 执行一次测距（阻塞式）
 */
bsp_status_t dev_hcsr04_read_mm(uint32_t *distance_mm)
{
    if (distance_mm == NULL)
    {
        return BSP_EINVAL;
    }

    *distance_mm = 0U;

    /* 1. 触发：TRIG 拉高 ≥10µs 后拉低 */
    (void)port_gpio_write(PORT_GPIO_HCSR04_TRIG, PORT_GPIO_HIGH);
    port_dwt_delay_us(HCSR04_TRIG_PULSE_US);
    (void)port_gpio_write(PORT_GPIO_HCSR04_TRIG, PORT_GPIO_LOW);

    /* 2. 等待回波上升沿（模块内部可能有固定延时） */
    uint32_t elapsed = 0U;
    bsp_status_t ret = s_hcsr04_wait_level(PORT_GPIO_HIGH, HCSR04_ECHO_TIMEOUT_US, &elapsed);
    if (ret != BSP_OK)
    {
        return ret;
    }
    uint32_t echo_start = port_dwt_get_cycles();

    /* 3. 测量回波高电平持续时间 */
    ret = s_hcsr04_wait_level(PORT_GPIO_LOW, HCSR04_ECHO_TIMEOUT_US, &elapsed);
    if (ret != BSP_OK)
    {
        return ret;
    }
    uint32_t echo_us = (port_dwt_get_cycles() - echo_start) / HCSR04_CPU_FREQ_MHZ;

    /* 4. 换算距离（毫米） */
    *distance_mm = (echo_us * HCSR04_MM_PER_US_NUM) / HCSR04_MM_PER_US_DEN;

    return BSP_OK;
}
