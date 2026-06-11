/**
 * @file dev_st7789.h
 * @brief ST7789 LCD 液晶屏驱动头文件
 * @note 对外接口统一采用 lcd_ 前缀，便于后续更换显示芯片时保持上层接口一致。
 */

#ifndef __DEV_ST7789_H
#define __DEV_ST7789_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ST7789 物理分辨率与坐标偏移配置 */
#ifndef ST7789_CFG_DEFAULT_WIDTH
#define ST7789_CFG_DEFAULT_WIDTH 240
#endif

#ifndef ST7789_CFG_DEFAULT_HEIGHT
#define ST7789_CFG_DEFAULT_HEIGHT 320
#endif

#ifndef ST7789_CFG_X_OFFSET
#define ST7789_CFG_X_OFFSET 0
#endif

#ifndef ST7789_CFG_Y_OFFSET
#define ST7789_CFG_Y_OFFSET 0
#endif

/* RGB565 常用颜色宏定义 */
#define LCD_COLOR_WHITE     0xFFFF
#define LCD_COLOR_BLACK     0x0000
#define LCD_COLOR_RED       0xF800
#define LCD_COLOR_GREEN     0x07E0
#define LCD_COLOR_BLUE      0x001F
#define LCD_COLOR_YELLOW    0xFFE0
#define LCD_COLOR_CYAN      0x07FF
#define LCD_COLOR_MAGENTA   0xF81F
#define LCD_COLOR_GRAY      0x8430
#define LCD_COLOR_ORANGE    0xFD20
#define LCD_COLOR_PINK      0xF8B2
#define LCD_COLOR_PURPLE    0x8010
#define LCD_COLOR_BROWN     0xA145
#define LCD_COLOR_DARKBLUE  0x01CF
#define LCD_COLOR_LIGHTBLUE 0x7D7C

/* DMA 异步传输完成回调函数指针类型 */
typedef void (*lcd_dma_cb_t)(bool success, void *user_data);

/* -------------------------------------------------------------------------
 * 导出 API 函数声明
 * ------------------------------------------------------------------------- */

void lcd_init(void);
void lcd_set_rotation(uint8_t rotation);
uint8_t lcd_get_rotation(void);

void lcd_display_on(void);
void lcd_display_off(void);

uint16_t lcd_get_width(void);
uint16_t lcd_get_height(void);

/* 基础绘图与同步刷新接口 */
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill_color(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *p_data);

/* DMA 异步刷新接口 */
bsp_status_t lcd_flush_async(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *p_data);
bsp_status_t lcd_flush_async_cb(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t *p_data, lcd_dma_cb_t done_cb, void *user_data);

bool lcd_is_busy(void);
void lcd_wait_done(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_ST7789_H */
