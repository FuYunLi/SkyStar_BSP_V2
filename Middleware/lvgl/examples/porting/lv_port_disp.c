/**
 * @file lv_port_disp.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 * ********************/
#include "lv_port_disp.h"
#include "dev_st7789.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 * ********************/
#ifndef MY_DISP_HOR_RES
#warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
#define MY_DISP_HOR_RES 240
#endif

#ifndef MY_DISP_VER_RES
#warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
#define MY_DISP_VER_RES 320
#endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 * **********************/

/**********************
 *  STATIC PROTOTYPES
 * ********************/
static void disp_init(void);

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

static void lv_flush_done_cb(bool success, void *user_data);

/**********************
 *  STATIC VARIABLES
 * **********************/

/**********************
 *      MACROS
 * **********************/

/**********************
 *   GLOBAL FUNCTIONS
 * **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    /* 设置显示屏的色彩格式为字节反转格式以解决大端液晶屏的色彩模糊问题 */
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_1[MY_DISP_HOR_RES * 40 * BYTE_PER_PIXEL];

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_2[MY_DISP_HOR_RES * 40 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

}

/**********************
 *   STATIC FUNCTIONS
 * **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    lcd_init();
    lcd_set_rotation(0);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/* DMA 异步刷新完成后的中断回调函数 */
static void lv_flush_done_cb(bool success, void *user_data)
{
    (void)success;
    lv_display_t *disp = (lv_display_t *)user_data;
    lv_display_flush_ready(disp);
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (disp_flush_enabled)
    {
        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

        /* 计算刷新区域的宽度和高度 */
        int32_t width  = area->x2 - area->x1 + 1;
        int32_t height = area->y2 - area->y1 + 1;

        /* 参数合法性检查 */
        if (width <= 0 || height <= 0)
        {
            lv_display_flush_ready(disp);
            return;
        }

        /* 优先尝试通过 DMA 异步刷新数据到液晶屏 */
        bsp_status_t ret = lcd_flush_async_cb((uint16_t)area->x1, (uint16_t)area->y1, (uint16_t)area->x2,
                                              (uint16_t)area->y2, (uint16_t *)px_map, lv_flush_done_cb, disp);

        if (ret == BSP_OK)
        {
            return;
        }

        /* 若 DMA 忙，则回退到阻塞式同步刷新以确保画面正常送达 */
        lcd_flush((uint16_t)area->x1, (uint16_t)area->y1, (uint16_t)area->x2, (uint16_t)area->y2, (uint16_t *)px_map);
        lv_display_flush_ready(disp);
    }
    else
    {
        /*IMPORTANT!!!
         *Inform the graphics library that you are ready with the flushing*/
        lv_display_flush_ready(disp);
    }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif