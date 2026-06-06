/**
 * @file app_rgb_demo.c
 * @brief WS2812B RGB LED 演示模块
 * @note 导出控制颜色的命令到 Letter Shell，含彩虹和呼吸特效。
 */

#include "app_rgb_demo.h"
#include "dev_ws2812.h"
#include "port_tick.h"
#include "shell.h"
#define LOG_TAG "RGB_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief 简单的 HSV 到 RGB 转换函数，用于生成彩虹效果
 */
static dev_ws2812_rgb_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    dev_ws2812_rgb_t rgb = {0, 0, 0};
    uint8_t region, remainder, p, q, t;

    if (s == 0)
    {
        rgb.r = v;
        rgb.g = v;
        rgb.b = v;
        return rgb;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = v;
            break;
        default:
            rgb.r = v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}

int shell_rgb_set(int argc, char *argv[])
{
    if (argc < 4)
    {
        log_e("Usage: rgb_set <R> <G> <B>");
        return -1;
    }

    int r, g, b;
    sscanf(argv[1], "%d", &r);
    sscanf(argv[2], "%d", &g);
    sscanf(argv[3], "%d", &b);

    dev_ws2812_rgb_t color = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
    dev_ws2812_set_all(color);
    dev_ws2812_refresh();
    
    log_i("RGB set to (%d, %d, %d)", r, g, b);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, rgb_set, shell_rgb_set, "Set RGB color");

int shell_rgb_brightness(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: rgb_brightness <0-100>");
        return -1;
    }

    int percent;
    sscanf(argv[1], "%d", &percent);
    
    dev_ws2812_set_global_brightness((uint8_t)percent);
    dev_ws2812_refresh(); /* 重新刷新当前颜色缓存并应用亮度 */
    
    log_i("RGB global brightness set to %d%%", percent);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, rgb_brightness, shell_rgb_brightness, "Set RGB global brightness (0-100)");

void shell_rgb_rainbow(void)
{
    log_i("Starting rainbow effect (blocking for 5 seconds)...");
    
    for (uint16_t loop = 0; loop < 250; loop++)
    {
        for (uint16_t i = 0; i < DEV_WS2812_LED_COUNT; i++)
        {
            uint16_t h = (loop * 5 + i * 85) % 256;
            dev_ws2812_rgb_t color = hsv_to_rgb(h, 255, 50); /* 降低亮度到50，防止过亮 */
            dev_ws2812_set_color(i, color);
        }
        dev_ws2812_refresh();
        port_tick_delay_ms(20);
    }
    
    dev_ws2812_set_all(DEV_WS2812_COLOR_BLACK);
    dev_ws2812_refresh();
    log_i("Rainbow effect finished.");
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, rgb_rainbow, shell_rgb_rainbow, "RGB rainbow effect");

void shell_rgb_breathe(void)
{
    log_i("Starting breathe effect (blocking for 5 seconds)...");
    
    for (uint16_t loop = 0; loop < 250; loop++)
    {
        /* 简单的三角波模拟呼吸，周期大约 2 秒 (100 * 20ms) */
        uint8_t brightness = loop % 100;
        if (brightness > 50)
        {
            brightness = 100 - brightness;
        }
        
        dev_ws2812_rgb_t color = {brightness * 2, 0, brightness * 2}; /* 紫色呼吸 */
        dev_ws2812_set_all(color);
        dev_ws2812_refresh();
        port_tick_delay_ms(20);
    }
    
    dev_ws2812_set_all(DEV_WS2812_COLOR_BLACK);
    dev_ws2812_refresh();
    log_i("Breathe effect finished.");
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, rgb_breathe, shell_rgb_breathe, "RGB breathe effect");

bsp_status_t app_rgb_demo_init(void)
{
    log_i("[APP] RGB demo module initialized successfully");
    return BSP_OK;
}
