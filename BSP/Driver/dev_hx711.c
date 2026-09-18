/**
 * @file dev_hx711.c
 * @brief HX711 24 位称重 ADC 驱动实现
 * @note 通信时序（对照数据手册与官方出厂 hx711_driver.c）：
 *       DOUT 拉低表示数据就绪；随后 SCK 输出 25 个正脉冲，
 *       前 24 个移出补码数据（MSB 先行），第 25 个选择下次增益
 *       （本驱动固定 128 增益）。
 *       关键约束：SCK 高电平持续超过 60µs 会导致芯片进入断电
 *       复位模式，故读取过程处于临界区，且不可在中断中调用。
 *       【未验证】外部称重传感器尚未接线实测。
 */

#include "dev_hx711.h"
#include "port_gpio.h"
#include "port_critical.h"
#include "port_dwt.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define HX711_READY_TIMEOUT_MS  (150U)  /* DOUT 就绪等待上限（10Hz 转换率） */
#define HX711_READY_POLL_US     (100U)  /* 就绪轮询间隔 */
#define HX711_PULSE_US          (1U)    /* SCK 脉宽，须远小于 60µs 断电阈值 */

/* 默认校准参数：与官方出厂例程一致，实际需按传感器标定 */
#define HX711_DEFAULT_OFFSET    (2675)
#define HX711_DEFAULT_SCALE     (211.875f)

/* ================================================================
 * 私有变量
 * ================================================================ */

static int32_t s_hx711_offset = HX711_DEFAULT_OFFSET;
static float s_hx711_scale = HX711_DEFAULT_SCALE;

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 HX711
 */
bsp_status_t dev_hx711_init(void)
{
    /* 引脚模式与初始电平由 port_gpio_init 统一配置，此处仅复位状态 */
    s_hx711_offset = HX711_DEFAULT_OFFSET;
    s_hx711_scale = HX711_DEFAULT_SCALE;

    return BSP_OK;
}

/**
 * @brief 读取一次 24 位原始值
 */
bsp_status_t dev_hx711_read_raw(int32_t *raw)
{
    if (raw == NULL)
    {
        return BSP_EINVAL;
    }

    /* 1. 等待 DOUT 变低（数据就绪），按物理延时轮询防优化踩坑 */
    uint32_t start = HAL_GetTick();
    port_gpio_state_t dout = PORT_GPIO_HIGH;

    while (dout == PORT_GPIO_HIGH)
    {
        (void)port_gpio_read(PORT_GPIO_HX711_DOUT, &dout);

        if ((HAL_GetTick() - start) >= HX711_READY_TIMEOUT_MS)
        {
            return BSP_ETIMEOUT;
        }

        port_dwt_delay_us(HX711_READY_POLL_US);
    }

    /* 2. 位读取全程临界区：任何中断拉长 SCK 高电平都会触发断电复位 */
    uint32_t primask = port_enter_critical();

    uint32_t raw_data = 0U;

    for (uint32_t i = 0U; i < 24U; i++)
    {
        (void)port_gpio_write(PORT_GPIO_HX711_SCK, PORT_GPIO_HIGH);
        port_dwt_delay_us(HX711_PULSE_US);

        raw_data <<= 1;

        (void)port_gpio_write(PORT_GPIO_HX711_SCK, PORT_GPIO_LOW);
        port_dwt_delay_us(HX711_PULSE_US);

        (void)port_gpio_read(PORT_GPIO_HX711_DOUT, &dout);
        if (dout == PORT_GPIO_HIGH)
        {
            raw_data |= 1UL;
        }
    }

    /* 3. 第 25 个脉冲：选择下次转换增益 128 */
    (void)port_gpio_write(PORT_GPIO_HX711_SCK, PORT_GPIO_HIGH);
    port_dwt_delay_us(HX711_PULSE_US);
    (void)port_gpio_write(PORT_GPIO_HX711_SCK, PORT_GPIO_LOW);
    port_dwt_delay_us(HX711_PULSE_US);

    port_exit_critical(primask);

    /* 4. 24 位补码符号扩展到 32 位 */
    *raw = (raw_data & 0x800000UL) ? (int32_t)(raw_data | 0xFF000000UL) : (int32_t)raw_data;

    return BSP_OK;
}

/**
 * @brief 设置皮重偏移
 */
bsp_status_t dev_hx711_set_offset(int32_t offset)
{
    s_hx711_offset = offset;
    return BSP_OK;
}

/**
 * @brief 设置刻度系数
 */
bsp_status_t dev_hx711_set_scale(float scale)
{
    if (scale <= 0.0f)
    {
        return BSP_EINVAL;
    }

    s_hx711_scale = scale;
    return BSP_OK;
}

/**
 * @brief 读取净重
 */
bsp_status_t dev_hx711_read_weight_g(float *weight_g)
{
    if (weight_g == NULL)
    {
        return BSP_EINVAL;
    }

    int32_t raw = 0;
    bsp_status_t ret = dev_hx711_read_raw(&raw);
    if (ret != BSP_OK)
    {
        return ret;
    }

    *weight_g = ((float)raw - (float)s_hx711_offset) / s_hx711_scale;

    return BSP_OK;
}
