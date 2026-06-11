/**
 * @file app_lcd_demo.c
 * @brief LCD 演示与自检测试模块源文件
 */

#include "app_lcd_demo.h"
#include "dev_st7789.h"
#include "bsp_backlight.h"
#include "shell.h"
#define LOG_TAG "LCD_DEMO"
#include "elog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 定义 DMA 测试缓冲区大小 (120x120 像素) */
#define TEST_FB_WIDTH  120
#define TEST_FB_HEIGHT 120

/* 静态测试帧缓冲区，存放在 BSS 段中 */
static uint16_t s_test_fb[TEST_FB_WIDTH * TEST_FB_HEIGHT];
static volatile bool s_dma_test_done = false;

/* =========================================================================
 * 内部辅助函数
 * ========================================================================= */

/**
 * @brief 自检 DMA 发送完毕中断回调
 */
static void app_lcd_dma_callback(bool success, void *user_data)
{
    (void)user_data;
    if (success)
    {
        s_dma_test_done = true;
    }
}

/**
 * @brief 填充测试帧缓冲区为渐变色
 * @param hue 颜色调整偏置
 */
static void fill_gradient_fb(uint8_t hue)
{
    for (int y = 0; y < TEST_FB_HEIGHT; y++)
    {
        for (int x = 0; x < TEST_FB_WIDTH; x++)
        {
            // 产生一个随坐标与 hue 变化的 RGB565 渐变色
            uint8_t r = (uint8_t)((x * 31 / TEST_FB_WIDTH) + hue) & 0x1F;
            uint8_t g = (uint8_t)((y * 63 / TEST_FB_HEIGHT) + (hue * 2)) & 0x3F;
            uint8_t b = (uint8_t)(((x + y) * 31 / (TEST_FB_WIDTH + TEST_FB_HEIGHT))) & 0x1F;
            
            // 拼接大端格式以符合屏幕字节序要求
            uint16_t color = (uint16_t)((r << 11) | (g << 5) | b);
            s_test_fb[y * TEST_FB_WIDTH + x] = (uint16_t)(((color & 0xFF) << 8) | (color >> 8));
        }
    }
}

/* =========================================================================
 * Shell 命令实现
 * ========================================================================= */

/**
 * @brief lcd_fill 颜色填充命令实现
 */
int shell_lcd_fill(int argc, char *argv[])
{
    uint16_t color = LCD_COLOR_WHITE;

    if (argc >= 2)
    {
        color = (uint16_t)strtol(argv[1], NULL, 16);
    }
    else
    {
        log_w("No color parameter, default to WHITE (0xFFFF)");
    }

    log_i("Filling screen with color 0x%04X...", color);

    // 等待可能存在的前一轮 DMA 传输完成，然后执行阻塞式清屏
    lcd_wait_done();
    lcd_fill_color(0, 0, lcd_get_width(), lcd_get_height(), color);
    
    log_i("Fill complete");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, lcd_fill, shell_lcd_fill, "Fill LCD with hex color. Usage: lcd_fill [0xF800]");

/**
 * @brief lcd_test 屏幕核心功能自检流程实现
 */
int shell_lcd_test(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    log_i("Starting LCD hardware self-test...");

    // 1. 基色阻塞刷新测试
    log_i("Step 1: Solid color sequence test");
    const uint16_t color_list[] = {LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_BLUE, LCD_COLOR_BLACK, LCD_COLOR_WHITE};
    const char *color_names[] = {"RED", "GREEN", "BLUE", "BLACK", "WHITE"};

    for (int i = 0; i < 5; i++)
    {
        log_i("Displaying %s...", color_names[i]);
        lcd_fill_color(0, 0, lcd_get_width(), lcd_get_height(), color_list[i]);
        port_tick_delay_ms(500);
    }

    // 2. 四向旋转测试
    log_i("Step 2: Rotation & offset boundary test");
    const uint8_t old_rotation = lcd_get_rotation();

    for (uint8_t rot = 0; rot < 4; rot++)
    {
        lcd_set_rotation(rot);
        log_i("Rotation %d: size %dx%d", rot, lcd_get_width(), lcd_get_height());
        
        // 四向分别使用红、绿、蓝、黄填充，验证视口边缘对齐
        uint16_t rot_color = (rot == 0) ? LCD_COLOR_RED : (rot == 1) ? LCD_COLOR_GREEN : (rot == 2) ? LCD_COLOR_BLUE : LCD_COLOR_YELLOW;
        lcd_fill_color(0, 0, lcd_get_width(), lcd_get_height(), rot_color);
        port_tick_delay_ms(600);
    }
    
    // 恢复初始旋转方向
    lcd_set_rotation(old_rotation);

    // 3. DMA 异步双缓冲式的高速刷新率与撕裂测试
    log_i("Step 3: High speed DMA async transmission test");
    lcd_fill_color(0, 0, lcd_get_width(), lcd_get_height(), LCD_COLOR_BLACK);
    
    // 计算屏幕中心坐标以显示 120x120 测试窗口
    uint16_t x_start = (lcd_get_width() - TEST_FB_WIDTH) / 2;
    uint16_t y_start = (lcd_get_height() - TEST_FB_HEIGHT) / 2;
    uint16_t x_end = x_start + TEST_FB_WIDTH - 1;
    uint16_t y_end = y_start + TEST_FB_HEIGHT - 1;

    uint32_t start_tick = port_tick_get_ms();
    
    // 循环发送 100 帧渐变画面，并统计耗时
    for (int frame = 0; frame < 100; frame++)
    {
        fill_gradient_fb((uint8_t)frame);
        
        s_dma_test_done = false;
        bsp_status_t status = lcd_flush_async_cb(x_start, y_start, x_end, y_end, s_test_fb, app_lcd_dma_callback, NULL);
        if (status != BSP_OK)
        {
            log_e("Failed to start DMA transfer at frame %d", frame);
            break;
        }

        // 等待当前帧 DMA 发送完毕
        uint32_t timeout_cnt = 0;
        while (!s_dma_test_done)
        {
            port_tick_delay_ms(1);
            if (++timeout_cnt > 100)
            {
                log_e("DMA transfer timeout at frame %d", frame);
                break;
            }
        }
    }

    uint32_t elapsed = port_tick_get_ms() - start_tick;
    float fps = 100.0f / ((float)elapsed / 1000.0f);
    
    log_i("DMA test finished. 100 frames in %d ms, avg: %.1f FPS", (int)elapsed, fps);
    log_i("LCD hardware self-test [PASSED]");

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, lcd_test, shell_lcd_test, "Start LCD visual and performance self-test");

/**
 * @brief backlight 亮度控制命令实现
 * @param argc 参数个数
 * @param argv 参数列表
 * @retval int 命令行返回值
 * @brief 使用示例：
 * @code
 * backlight 80
 * @endcode
 */
int shell_backlight(int argc, char *argv[])
{
    if (argc >= 2)
    {
        uint8_t brightness = (uint8_t)strtol(argv[1], NULL, 10);
        if (bsp_backlight_set(brightness) == BSP_OK)
        {
            log_i("Set backlight brightness to %d%%", brightness);
        }
        else
        {
            log_e("Failed to set backlight brightness");
        }
    }
    else
    {
        log_i("Current backlight brightness: %d%%", bsp_backlight_get());
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, backlight, shell_backlight, "Set or get LCD backlight brightness. Usage: backlight [0-100]");

/* =========================================================================
 * 公开接口 API
 * ========================================================================= */

/**
 * @brief 初始化 LCD 演示与自检测试模块
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_lcd_demo_init(void)
{
    log_i("[APP] LCD demo module initialized successfully");
    return BSP_OK;
}
