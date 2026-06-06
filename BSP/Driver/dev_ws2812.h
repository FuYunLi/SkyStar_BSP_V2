/**
 * @file dev_ws2812.h
 * @brief WS2812B RGB LED 驱动头文件
 * @note 使用 PWM+DMA 的方式驱动，对外隐藏定时器与 DMA 细节。
 */

#ifndef __DEV_WS2812_H
#define __DEV_WS2812_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 物理板载的 WS2812 LED 数量 */
#define DEV_WS2812_LED_COUNT 3U

/**
 * @brief RGB 颜色结构体
 * @note 定义为 r, g, b 以保证易读性，底层自动转换为 GRB 的 24bit 数据流。
 */
typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} dev_ws2812_rgb_t;

/* 全局预置颜色定义 */
extern const dev_ws2812_rgb_t DEV_WS2812_COLOR_RED;
extern const dev_ws2812_rgb_t DEV_WS2812_COLOR_GREEN;
extern const dev_ws2812_rgb_t DEV_WS2812_COLOR_BLUE;
extern const dev_ws2812_rgb_t DEV_WS2812_COLOR_BLACK; /* 黑色 / 关闭 */
extern const dev_ws2812_rgb_t DEV_WS2812_COLOR_WHITE;

/* 接口函数 */
bsp_status_t dev_ws2812_init(void);
void dev_ws2812_set_global_brightness(uint8_t percent);
bsp_status_t dev_ws2812_set_color(uint16_t index, dev_ws2812_rgb_t color);
bsp_status_t dev_ws2812_set_all(dev_ws2812_rgb_t color);
bsp_status_t dev_ws2812_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_WS2812_H */
