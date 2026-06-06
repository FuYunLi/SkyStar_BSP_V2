/**
 * @file dev_ws2812.c
 * @brief WS2812B RGB LED 驱动实现
 * @note 采用 PWM+DMA 尾随 0 的方式发送，无需 HAL 中断回调。
 */

#include "dev_ws2812.h"
#include "port_pwm.h"

/* 定义常用预置颜色 */
const dev_ws2812_rgb_t DEV_WS2812_COLOR_RED   = {255, 0, 0};
const dev_ws2812_rgb_t DEV_WS2812_COLOR_GREEN = {0, 255, 0};
const dev_ws2812_rgb_t DEV_WS2812_COLOR_BLUE  = {0, 0, 255};
const dev_ws2812_rgb_t DEV_WS2812_COLOR_BLACK = {0, 0, 0};
const dev_ws2812_rgb_t DEV_WS2812_COLOR_WHITE = {255, 255, 255};

/* WS2812 通信时序占空比对应的 CCR 值 */
static uint32_t s_bit_0_ccr = 0;
static uint32_t s_bit_1_ccr = 0;

/* 
 * 缓冲数组长度: 
 * - 前置 50 个 0，确保发送起始有足够的静默时间，防止首 bit 波形畸变
 * - 3 颗灯 × 24 bit/灯 = 72 半字 (PWM 比较值)
 * - 尾随 50 个 0，产生超过 50us 的复位信号 (50 * 1.25us = 62.5us)
 * DMA 会一次性搬运全部数据
 */
#define WS2812_RESET_HEAD     (300U)
#define WS2812_BIT_COUNT      (DEV_WS2812_LED_COUNT * 24U)
#define WS2812_RESET_TAIL     (200U)
#define WS2812_DMA_BUF_LEN    (WS2812_RESET_HEAD + WS2812_BIT_COUNT + WS2812_RESET_TAIL)

static uint32_t s_dma_buffer[WS2812_DMA_BUF_LEN] = {0};
static uint8_t s_global_brightness = 100; /* 全局亮度百分比 (0~100) */

/**
 * @brief 初始化 WS2812 驱动
 * @retval BSP_OK 成功
 * @retval BSP_ERROR 失败
 */
bsp_status_t dev_ws2812_init(void)
{
    /* 1. 初始化 PWM 底层外设 */
    if (port_pwm_init(PORT_PWM_WS2812) != BSP_OK)
    {
        return BSP_ERROR;
    }

    /* 2. 强制设置 PWM 频率为 800kHz */
    if (port_pwm_set_freq(PORT_PWM_WS2812, 800000) != BSP_OK)
    {
        return BSP_ERROR;
    }

    /* 3. 动态计算占空比 CCR 值
     * WS2812B 协议：
     * 0 码: ~30% 占空比
     * 1 码: ~60% 占空比 
     */
    uint32_t arr = port_pwm_get_arr(PORT_PWM_WS2812);
    if (arr == 0)
    {
        return BSP_ERROR;
    }
    
    s_bit_0_ccr = (arr * 30) / 100;
    s_bit_1_ccr = (arr * 60) / 100;

    /* 4. 清空 DMA 缓冲区并刷新一次 */
    dev_ws2812_set_all(DEV_WS2812_COLOR_BLACK);
    return dev_ws2812_refresh();
}

/**
 * @brief 设置全局亮度
 * @param percent 亮度百分比 (0~100)
 */
void dev_ws2812_set_global_brightness(uint8_t percent)
{
    if (percent > 100)
    {
        percent = 100;
    }
    s_global_brightness = percent;
}

/**
 * @brief 设置单颗 WS2812 颜色（不立即生效）
 * @param index 灯珠索引（0 ~ DEV_WS2812_LED_COUNT-1）
 * @param color RGB 颜色数据
 * @retval BSP_OK 成功
 * @retval BSP_EINVAL 索引越界
 */
bsp_status_t dev_ws2812_set_color(uint16_t index, dev_ws2812_rgb_t color)
{
    if (index >= DEV_WS2812_LED_COUNT)
    {
        return BSP_EINVAL;
    }

    /* 应用全局亮度缩放 */
    uint8_t r = ((uint16_t)color.r * s_global_brightness) / 100;
    uint8_t g = ((uint16_t)color.g * s_global_brightness) / 100;
    uint8_t b = ((uint16_t)color.b * s_global_brightness) / 100;

    /* WS2812 物理协议按 G、R、B 的顺序发送 */
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | ((uint32_t)b);
    uint16_t buf_index = WS2812_RESET_HEAD + index * 24;

    /* 提取 24 位并填入 DMA 缓存 (高位先发) */
    for (int i = 0; i < 24; i++)
    {
        if (grb & (1 << (23 - i)))
        {
            s_dma_buffer[buf_index + i] = s_bit_1_ccr;
        }
        else
        {
            s_dma_buffer[buf_index + i] = s_bit_0_ccr;
        }
    }

    return BSP_OK;
}

/**
 * @brief 设置所有 WS2812 颜色（不立即生效）
 * @param color RGB 颜色数据
 * @retval BSP_OK 成功
 */
bsp_status_t dev_ws2812_set_all(dev_ws2812_rgb_t color)
{
    for (uint16_t i = 0; i < DEV_WS2812_LED_COUNT; i++)
    {
        dev_ws2812_set_color(i, color);
    }
    return BSP_OK;
}

/**
 * @brief 刷新数据到 WS2812 灯带
 * @note 内部调用 DMA 发送数据
 * @retval BSP_OK 启动发送成功
 */
bsp_status_t dev_ws2812_refresh(void)
{
    /* 先停止可能存在的旧 DMA 发送 */
    port_pwm_dma_stop(PORT_PWM_WS2812);
    
    /* 开启新的 DMA 发送 */
    return port_pwm_dma_start(PORT_PWM_WS2812, s_dma_buffer, WS2812_DMA_BUF_LEN);
}
